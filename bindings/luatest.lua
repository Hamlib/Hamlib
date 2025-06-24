#!/usr/bin/lua

Hamlib = require("Hamliblua")

-- you can see the Hamlib properties:
-- for key,value in pairs(Hamlib) do print(key,value) end

function version()
    ver = string.sub(_VERSION,4)
    -- should only get one match to this
    for ver2 in string.gmatch(ver,"%d.%d") do
        ver = tonumber(ver2)
    end
    return ver
end


function doStartup()
    print(string.format("%s test, %s\n", _VERSION, Hamlib.hamlib_version))


    -- Hamlib.rig_set_debug (Hamlib.RIG_DEBUG_TRACE)
    Hamlib.rig_set_debug (Hamlib.RIG_DEBUG_NONE)

    -- Init RIG_MODEL_DUMMY
    my_rig = Hamlib.Rig (Hamlib.RIG_MODEL_DUMMY)
    -- you can see the Rig properties:
    -- for key,value in pairs(my_rig) do print(key,value) end

    my_rig:set_conf("rig_pathname", "/dev/Rig")

    my_rig:set_conf("retry", "5")
    my_rig:open()

    tpath = my_rig:get_conf("rig_pathname")
    retry = my_rig:get_conf("retry")

    print (string.format("status(str):\t\t%s", Hamlib.rigerror(my_rig.error_status)))
    print (string.format("get_conf:\t\tpath = %s, retry = %s", tpath, retry))

    my_rig:set_freq(Hamlib.RIG_VFO_B, 5700000000)
    my_rig:set_vfo(Hamlib.RIG_VFO_B)
    print(string.format("freq:\t\t\t%d", my_rig:get_freq()))
    my_rig:set_freq(Hamlib.RIG_VFO_A, 145550000)
    my_rig:set_vfo(Hamlib.RIG_VFO_A)

    mode, width = my_rig:get_mode()
    print(string.format("mode:\t\t\t%s\nbandwidth:\t\t%s", Hamlib.rig_strrmode(mode), width))

    my_rig:set_mode(Hamlib.RIG_MODE_CW)
    mode, width = my_rig:get_mode()
    print(string.format("mode:\t\t\t%s\nbandwidth:\t\t%d", Hamlib.rig_strrmode(mode), width))

    print(string.format("Backend copyright:\t%s",my_rig.caps.copyright))

    print(string.format("Model:\t\t\t%s", my_rig.caps.model_name))
    print(string.format("Manufacturer:\t\t%s", my_rig.caps.mfg_name))
    print(string.format("Backend version:\t%s", my_rig.caps.version))
    print(string.format("Backend license:\t%s", my_rig.caps.copyright))
    print(string.format("Rig info:\t\t%s", my_rig:get_info()))

    my_rig:set_level("VOXDELAY", 1)
    print(string.format("status:\t\t\t%s - %s", my_rig.error_status, Hamlib.rigerror(my_rig.error_status)))
    print(string.format("VOX delay:\t\t%d", my_rig:get_level_i("VOXDELAY")))
    my_rig:set_level(Hamlib.RIG_LEVEL_VOXDELAY, 5)
    print(string.format("status:\t\t\t%s - %s", my_rig.error_status, Hamlib.rigerror(my_rig.error_status)))
    -- see the func name (get_level_i) and format (%d)
    print(string.format("VOX delay:\t\t%d", my_rig:get_level_i(Hamlib.RIG_LEVEL_VOXDELAY)))

    af = 12.34
    print(string.format("Setting AF to %f...", af))
    my_rig:set_level(Hamlib.RIG_LEVEL_AF, af)
    print(string.format("status:\t\t\t%s - %s", my_rig.error_status, Hamlib.rigerror(my_rig.error_status)))
    -- see the different of func name and format related to RIG_LEVEL_VOXDELAY!
    print(string.format("AF level:\t\t%f", my_rig:get_level_f("AF")))

    print(string.format("strength:\t\t%d", my_rig:get_level_i(Hamlib.RIG_LEVEL_STRENGTH)))
    print(string.format("status:\t\t\t%s", my_rig.error_status))
    print(string.format("status(str):\t\t%s", Hamlib.rigerror(my_rig.error_status)))

    chan = Hamlib.channel(Hamlib.RIG_VFO_B)

    my_rig:get_channel(chan,1)
    print(string.format("get_channel status:\t%d", my_rig.error_status))

    print(string.format("VFO:\t\t\t%s, %s", Hamlib.rig_strvfo(chan.vfo), chan.freq))
    print(string.format("Attenuators:\t\t%s", table.concat(my_rig.caps.attenuator, ", ")))

    print("\nSending Morse, '73'")
    my_rig:send_morse(Hamlib.RIG_VFO_A, "73")

    my_rig:close ()

    print("\nSome static functions:")

    err, lon1, lat1 = Hamlib.locator2longlat("IN98XC")
    err, lon2, lat2 = Hamlib.locator2longlat("DM33DX")
    err, loc1 = Hamlib.longlat2locator(lon1, lat1, 3)
    err, loc2 = Hamlib.longlat2locator(lon2, lat2, 3)
    print(string.format("Loc1:\t\tIN98XC -> %9.4f, %9.4f -> %s", lon1, lat1, loc1))
    print(string.format("Loc2:\t\tDM33DX -> %9.4f, %9.4f -> %s", lon2, lat2, loc2))

    err, dist, az = Hamlib.qrb(lon1, lat1, lon2, lat2)
    longpath = Hamlib.distance_long_path(dist)
    print(string.format("Distance:\t%.3f km, azimuth %.2f, long path:\t%.3f km", dist, az, longpath))

    -- dec2dms expects values from 180 to -180
    -- sw is 1 when deg is negative (west or south) as 0 cannot be signed
    err, deg1, mins1, sec1, sw1 = Hamlib.dec2dms(lon1)
    err, deg2, mins2, sec2, sw2 = Hamlib.dec2dms(lat1)

    lon3 = Hamlib.dms2dec(deg1, mins1, sec1, sw1)
    lat3 = Hamlib.dms2dec(deg2, mins2, sec2, sw2)

    if sw1 > 0 then D = 'W' else D = 'E' end
    print(string.format("Longitude:\t%4.4f, %4.0f° %.0f' %2.0f\" %1s\trecoded: %9.4f", lon1, deg1, mins1, sec1, D, lon3))

    if sw2 > 0 then D = 'S' else D = 'N' end
    print(string.format("Latitude:\t%4.4f, %4.0f° %.0f' %2.0f\" %1s\trecoded: %9.4f", lat1, deg2, mins2, sec2, D, lat3))
    if (version() >= 5.4) then
        -- older version may not handle 64-bit values
        -- not sure when this was fixed...might have been 5.3 somewhere
        print(string.format("The next two lines should show 0x8000000000000000"));
        print(string.format("RIG_MODE_TESTS_MAX: 0x%08x", Hamlib.RIG_MODE_TESTS_MAX));
        print(string.format("RIG_FUNC_BIT63: 0x%08x", Hamlib.RIG_FUNC_BIT63));
    end

end

doStartup()





