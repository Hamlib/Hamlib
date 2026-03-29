#include <ctype.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hamlib/rig.h"
#include "register.h"
#include "iofunc.h"
#include "harris.h"
#include "prc138.h"

static const float bw_ssb[] = {1.5, 2.0, 2.4, 2.7, 3.0, 0};
static const float bw_cw[]  = {0.35, 0.68, 1.0, 1.5, 0};
static const float bw_am[]  = {3.0, 4.0, 5.0, 6.0, 0};

static int harris_transaction(RIG *rig, const char *cmd, char *resp, int resp_len)
{
    unsigned char buf[128];
    char local_buf[512];
    char *target_buf = resp ? resp : local_buf;
    int target_len = resp ? resp_len : sizeof(local_buf);
    int retry = 15;
    
    target_buf[0] = '\0';

    /* 1. Pulizia fisica del buffer */
    rig_flush(RIGPORT(rig));

    /* 2. Invio del comando */
    if (write_block(RIGPORT(rig), (unsigned char *)cmd, strlen(cmd)) < 0) {
        return -RIG_EIO;
    }

    /* 3. Loop di accumulo */
    while (retry > 0) {
        usleep(10000); //150000
        
        memset(buf, 0, sizeof(buf));
        (void) read_block(RIGPORT(rig), buf, sizeof(buf) - 1);
        
        if (buf[0] != '\0') {
            strncat(target_buf, (char *)buf, target_len - strlen(target_buf) - 1);
            
            /* Se troviamo il prompt con lo spazio, abbiamo finito */
            if (strstr(target_buf, "SSB> ")) {
                return RIG_OK;
            }
        }
        retry--;
    }
    return -RIG_EIO;
}

int harris_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    char cmd[64];
    
    rig_debug(RIG_DEBUG_VERBOSE, "%s: impostazione frequenza a %.0f Hz\n", __func__, freq);

    /* Il formato %08.0f è CRUCIALE: 
       - 0 significa 'riempi con zeri'
       - 8 significa 'lunghezza totale di 8 caratteri'
       - .0f significa 'numero reale senza decimali' 
       Esempio: 7000000 diventa 07000000 */
    snprintf(cmd, sizeof(cmd), "FR %08.0f\r", freq);

    /* Usiamo la nostra fidata harris_transaction */
    if (harris_transaction(rig, cmd, NULL, 0) != RIG_OK) {
        rig_debug(RIG_DEBUG_ERR, "%s: errore nella transazione FR\n", __func__);
        return -RIG_EIO;
    }

    return RIG_OK;
}

int harris_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
    char buf[512];
    char *p;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: richiesta frequenza\n", __func__);

    /* 1. Eseguiamo la transazione completa */
    if (harris_transaction(rig, "FR\r", buf, sizeof(buf)) != RIG_OK) {
        rig_debug(RIG_DEBUG_ERR, "%s: errore nella transazione FR\n", __func__);
        return -RIG_EIO;
    }

    /* 2. Cerchiamo la riga della frequenza di ricezione "RxFr " */
    p = strstr(buf, "RxFr ");
    if (p) {
        /* Saltiamo "RxFr " (5 caratteri) e leggiamo il numero */
        /* sscanf è più robusto di atof per stringhe che terminano con caratteri speciali */
        if (sscanf(p + 5, "%lf", freq) != 1) {
            rig_debug(RIG_DEBUG_ERR, "%s: formato frequenza non riconosciuto in [%s]\n", __func__, p);
            return -RIG_EPROTO;
        }
        
        rig_debug(RIG_DEBUG_VERBOSE, "%s: frequenza letta: %.0f Hz\n", __func__, *freq);
        return RIG_OK;
    }

    rig_debug(RIG_DEBUG_ERR, "%s: stringa RxFr non trovata nella risposta\n", __func__);
    return -RIG_EPROTO;
}

int harris_open(RIG *rig)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s: Apertura e sincronizzazione prompt\n", __func__);

    /* Inviamo un semplice ritorno a capo per sollecitare il prompt */
    /* Passiamo NULL come buffer di risposta perché ci interessa solo l'esito (RIG_OK) */
    if (harris_transaction(rig, "\r", NULL, 0) != RIG_OK) {
        rig_debug(RIG_DEBUG_ERR, "%s: La radio non risponde al prompt iniziale\n", __func__);
        return -RIG_EIO;
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s: Connessione stabilita con successo\n", __func__);
    return RIG_OK;
}

static float find_closest_bandwidth(float target, const float *list) {
    float closest = list[0];
    float min_diff = 99.0; // Valore iniziale alto

    for (int i = 0; list[i] != 0; i++) {
        float diff = target - list[i];
        if (diff < 0) diff = -diff; // Valore assoluto
        
        if (diff < min_diff) {
            min_diff = diff;
            closest = list[i];
        }
    }
    return closest;
}

int harris_set_pb_width(RIG *rig, vfo_t vfo, pbwidth_t width)
{
    char cmd[64];
    rmode_t current_mode;
    pbwidth_t current_width;
    const float *bw_list;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: richiesta variazione larghezza banda a %ld Hz\n", __func__, width);

    /* 1. Recuperiamo il modo attuale per sapere quale tabella usare */
    if (harris_get_mode(rig, vfo, &current_mode, &current_width) != RIG_OK) {
        return -RIG_EIO;
    }

    /* 2. Selezioniamo la tabella corretta */
    switch (current_mode) {
        case RIG_MODE_USB:
        case RIG_MODE_LSB: bw_list = bw_ssb; break;
        case RIG_MODE_CW:  bw_list = bw_cw;  break;
        case RIG_MODE_AM:  bw_list = bw_am;  break;
        default: bw_list = bw_ssb; // Fallback
    }

    /* 3. Approssimazione e invio */
    float target_khz = (float)width / 1000.0;
    float final_khz = find_closest_bandwidth(target_khz, bw_list);

    snprintf(cmd, sizeof(cmd), "BAND %.2f\r", final_khz);
    return harris_transaction(rig, cmd, NULL, 0);
}

int harris_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
    char cmd[64];
    const char *mstr;

    switch (mode) {
        case RIG_MODE_USB: mstr = "USB"; break;
        case RIG_MODE_LSB: mstr = "LSB"; break;
        case RIG_MODE_CW:  mstr = "CW";  break;
        case RIG_MODE_AM:  mstr = "AME"; break;
        default: return -RIG_EINVAL;
    }

    /* 1. Cambia il Modo */
    snprintf(cmd, sizeof(cmd), "MODE %s\r", mstr);
    if (harris_transaction(rig, cmd, NULL, 0) != RIG_OK) return -RIG_EIO;

    /* 2. Cambia la banda usando la nuova funzione (se width > 0) */
    if (width > 0 && width != RIG_PASSBAND_NORMAL) {
        return harris_set_pb_width(rig, vfo, width);
    }

    return RIG_OK;
}

int harris_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
{
    char buf[512];
    char *p;

    if (harris_transaction(rig, "MODE\r", buf, sizeof(buf)) != RIG_OK) return -RIG_EIO;

    /* Parsing Modo */
    p = strstr(buf, "MODE ");
    if (p) {
        p += 5;
        if (strncmp(p, "USB", 3) == 0)      *mode = RIG_MODE_USB;
        else if (strncmp(p, "LSB", 3) == 0) *mode = RIG_MODE_LSB;
        else if (strncmp(p, "CW", 2) == 0)  *mode = RIG_MODE_CW;
        else if (strncmp(p, "AME", 3) == 0 || strncmp(p, "AM", 2) == 0) *mode = RIG_MODE_AM;
    }

    /* Parsing Banda (Filtro) */
    p = strstr(buf, "BAND ");
    if (p) {
        *width = (pbwidth_t)(atof(p + 5) * 1000.0);
    }

    return RIG_OK;
}

int harris_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val)
{
    char buf[2048];

    /* Caso speciale per il Preamp: il comando SH non lo mostra nel dump */
    if (level == RIG_LEVEL_PREAMP) {
        if (harris_transaction(rig, "PRE\r", buf, sizeof(buf)) != RIG_OK) {
            return -RIG_EIO;
        }
        val->i = strstr(buf, "Preamp Enabled") ? 1 : 0;
        return RIG_OK;
    }

    /* Per tutti gli altri livelli usiamo il dump SH */
    if (harris_transaction(rig, "SH\r", buf, sizeof(buf)) != RIG_OK) {
        return -RIG_EIO;
    }

    switch (level) {
        case RIG_LEVEL_SQL:
            if (strstr(buf, "SQ_LEVEL LOW")) val->f = 0.1f;
            else if (strstr(buf, "SQ_LEVEL MED")) val->f = 0.5f;
            else if (strstr(buf, "SQ_LEVEL HIGH")) val->f = 1.0f;
            else val->f = 0.0f;
            break;

        case RIG_LEVEL_RFPOWER:
            if (strstr(buf, "POWER low")) val->f = 0.1f;
            else if (strstr(buf, "POWER med")) val->f = 0.5f;
            else if (strstr(buf, "POWER hi")) val->f = 1.0f;
            else val->f = 0.0f;
            break;

        case RIG_LEVEL_AGC:
            if (strstr(buf, "AGC OFF")) val->i = RIG_AGC_OFF;
            else if (strstr(buf, "AGC SLOW")) val->i = RIG_AGC_SLOW;
            else if (strstr(buf, "AGC MED")) val->i = RIG_AGC_MEDIUM;
            else if (strstr(buf, "AGC FAST")) val->i = RIG_AGC_FAST;
            break;

        case RIG_LEVEL_RF:
            {
                char *p = strstr(buf, "RFG ");
                if (p) val->f = (float)atoi(p + 4) / 100.0f;
                else val->f = 1.0f;
            }
            break;

        default:
            return -RIG_EINVAL;
    }

    return RIG_OK;
}
 
int harris_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val)
{
    char cmd[64];
    const char *p_str;

    switch (level) {
        case RIG_LEVEL_SQL:
            if (val.f <= 0.33f) p_str = "LOW";
            else if (val.f <= 0.66f) p_str = "MED";
            else p_str = "HIGH";
            snprintf(cmd, sizeof(cmd), "SQ_L %s\r", p_str);
            break;

        case RIG_LEVEL_RFPOWER:
            if (val.f <= 0.33f) p_str = "LOW";
            else if (val.f <= 0.66f) p_str = "MED";
            else p_str = "HIGH";
            snprintf(cmd, sizeof(cmd), "POW %s\r", p_str);
            break;

        case RIG_LEVEL_AGC:
            if (val.i == RIG_AGC_OFF) p_str = "OF";
            else if (val.i == RIG_AGC_SLOW) p_str = "SLOW";
            else if (val.i == RIG_AGC_MEDIUM) p_str = "MED";
            else if (val.i == RIG_AGC_FAST) p_str = "FAST";
            else p_str = "SLOW"; 
            snprintf(cmd, sizeof(cmd), "AGC %s\r", p_str); // Usiamo AGC esteso
            break;

        case RIG_LEVEL_RF:
            snprintf(cmd, sizeof(cmd), "RFG %d\r", (int)(val.f * 100));
            break;

        case RIG_LEVEL_PREAMP:
            // Cruciale: Solo "ENA" e "BYP" funzionano davvero
            snprintf(cmd, sizeof(cmd), "PRE %s\r", (val.i > 0) ? "ENA" : "BYP");
            break;

        default:
            return -RIG_EINVAL;
    }

    return harris_transaction(rig, cmd, NULL, 0);
} 

 
int harris_set_func(RIG *rig, vfo_t vfo, setting_t func, int status)
{
    char cmd[64];

    /* Supportiamo solo lo Squelch in questa funzione */
    if (func != RIG_FUNC_SQL) {
        rig_debug(RIG_DEBUG_TRACE, "%s: funzione %lu non supportata\n", __func__, func);
        return -RIG_EINVAL;
    }

    /* Comando Harris: SQ ON per attivare, SQ OF per disattivare */
    /* Nota: Usiamo 'OF' con una sola F come da logica dei tuoi test precedenti */
    snprintf(cmd, sizeof(cmd), "SQ %s\r", status ? "ON" : "OF");

    rig_debug(RIG_DEBUG_VERBOSE, "%s: impostazione Squelch status %d [%s]\n", 
              __func__, status, cmd);

    /* Usiamo la transazione centralizzata per attendere il prompt SSB> */
    return harris_transaction(rig, cmd, NULL, 0);
}

int harris_get_ptt(RIG *rig, vfo_t vfo, ptt_t *ptt)
{
    char buf[512];

    rig_debug(RIG_DEBUG_VERBOSE, "%s: query stato PTT\n", __func__);

    /* Usiamo la transazione centralizzata inviando solo 'K' */
    if (harris_transaction(rig, "K\r", buf, sizeof(buf)) != RIG_OK) {
        return -RIG_EIO;
    }

    /* Analisi della risposta nel buffer */
    /* La radio tipicamente risponde con "KEY ON", "KEY MIC" o "KEY OFF" */
    if (strstr(buf, "KEY ON") || strstr(buf, "KEY MIC")) {
        *ptt = RIG_PTT_ON;
    } else if (strstr(buf, "KEY OFF")) {
        *ptt = RIG_PTT_OFF;
    } else {
        /* Fallback di sicurezza basato su stringhe parziali */
        if (strstr(buf, "ON")) {
            *ptt = RIG_PTT_ON;
        } else {
            *ptt = RIG_PTT_OFF;
        }
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: PTT rilevato: %s\n", 
              __func__, (*ptt == RIG_PTT_ON) ? "ON" : "OFF");

    return RIG_OK;
}

int harris_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt)
{
    char cmd[16];

    /* Costruiamo il comando: K ON per attivare, K OF per disattivare */
    /* Nota: mantengo 'OF' con una sola F come da tua implementazione originale */
    snprintf(cmd, sizeof(cmd), "K %s\r", (ptt == RIG_PTT_ON) ? "ON" : "OF");

    rig_debug(RIG_DEBUG_VERBOSE, "%s: impostazione PTT [%s]\n", __func__, cmd);

    /* Invio tramite transazione (ignora la risposta testuale, cerca solo il prompt) */
    return harris_transaction(rig, cmd, NULL, 0);
}

int harris_init(RIG *rig) {
    struct harris_priv_data *priv;

    priv = (struct harris_priv_data *)calloc(1, sizeof(struct harris_priv_data));
    if (!priv) return -RIG_ENOMEM;

    rig->caps->priv = (void *)priv;
    return RIG_OK;
}

int harris_cleanup(RIG *rig) {
    if (rig->caps->priv) {
        free((void *)rig->caps->priv);
        rig->caps->priv = NULL;
    }
    return RIG_OK;
}

int harris_close(RIG *rig) {
    return RIG_OK;
}

/* This macro expands the initrigs5_harris function. */
DECLARE_INITRIG_BACKEND(harris)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s: _init called\n", __func__);
    rig_register((struct rig_caps *)&prc138_caps);
    return RIG_OK;
}
