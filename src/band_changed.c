// This is currently included in rig.c
// Can customize during build
// Eventually should improved this for external actions when
// rigctld gets integrated as a service within Hamlib
int HAMLIB_API rig_band_changed(RIG *rig, hamlib_bandselect_t band)
{
    // See band_changed.c
    // Examples:
    // rig_set_func(rig, RIG_VFO_CURR, RIG_FUNC_TUNER, 1);
    // rig_set_func(rig, RIG_VFO_CURR, RIG_FUNC_TUNER, 0);
    // value_t v;
    // rig_set_ant(rig, RIG_VFO_CURR, 1, v);
    switch (band)
    {
    case RIG_BANDSELECT_2200M:
        break;

    case RIG_BANDSELECT_600M:
        break;

    case RIG_BANDSELECT_160M:
        break;

    case RIG_BANDSELECT_80M:
        break;

    case RIG_BANDSELECT_60M:
        break;

    case RIG_BANDSELECT_40M:
        break;

    case RIG_BANDSELECT_30M:
        break;

    case RIG_BANDSELECT_20M:
        break;

    case RIG_BANDSELECT_17M:
        break;

    case RIG_BANDSELECT_15M:
        break;

    case RIG_BANDSELECT_12M:

        break;

    case RIG_BANDSELECT_10M:
        break;

    case RIG_BANDSELECT_6M:
        break;

    case RIG_BANDSELECT_WFM:
        break;

    case RIG_BANDSELECT_MW:
        break;

    case RIG_BANDSELECT_AIR:
        break;

    case RIG_BANDSELECT_2M:
        break;

    case RIG_BANDSELECT_1_25M:
        break;

    case RIG_BANDSELECT_70CM:
        break;

    case RIG_BANDSELECT_33CM:
        break;

    case RIG_BANDSELECT_23CM:
        break;

    case RIG_BANDSELECT_13CM:
        break;

    case RIG_BANDSELECT_9CM:
        break;

    case RIG_BANDSELECT_5CM:
        break;

    case RIG_BANDSELECT_3CM:
        break;

    case RIG_BANDSELECT_GEN:
        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: Unknown band=%d\n", __func__, band);
    }

    return RIG_OK;
}
