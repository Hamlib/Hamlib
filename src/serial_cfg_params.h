    {
        TOK_SERIAL_SPEED, "serial_speed", "Serial speed",
        "Serial port baud rate",
        "0", RIG_CONF_NUMERIC, { .n = { 300, 115200, 1 } }
    },
    {
        TOK_DATA_BITS, "data_bits", "Serial data bits",
        "Serial port data bits",
        "8", RIG_CONF_NUMERIC, { .n = { 5, 8, 1 } }
    },
    {
        TOK_STOP_BITS, "stop_bits", "Serial stop bits",
        "Serial port stop bits",
        "1", RIG_CONF_NUMERIC, { .n = { 0, 3, 1 } }
    },
    {
        TOK_PARITY, "serial_parity", "Serial parity",
        "Serial port parity",
        "None", RIG_CONF_COMBO, { .c = {{ "None", "Odd", "Even", "Mark", "Space", NULL }} }
    },
    {
        TOK_HANDSHAKE, "serial_handshake", "Serial handshake",
        "Serial port handshake",
        "None", RIG_CONF_COMBO, { .c = {{ "None", "XONXOFF", "Hardware", NULL }} }
    },

    {
        TOK_RTS_STATE, "rts_state", "RTS state",
        "Serial port set state of RTS signal for external powering",
        "Unset", RIG_CONF_COMBO, { .c = {{ "Unset", "ON", "OFF", NULL }} }
    },
    {
        TOK_DTR_STATE, "dtr_state", "DTR state",
        "Serial port set state of DTR signal for external powering",
        "Unset", RIG_CONF_COMBO, { .c = {{ "Unset", "ON", "OFF", NULL }} }
    },

    { RIG_CONF_END, NULL, }
