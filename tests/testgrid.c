#include <stdio.h>
#include <hamlib/rig.h>

int main(int argc, char *argv[])
{
    int retval;
    char *grid = NULL;
    double lat = 0, lon = 0;

    switch (argc)
    {
    case 1:
        grid = "EM49hv";
        break;

    case 2:
        grid = argv[1];
        break;

    case 3:
        sscanf(argv[1], "%lg", &lat);
        sscanf(argv[2], "%lg", &lon);
        break;

    default:
        fprintf(stderr, "Unknown # of arguments...expected 1 (grid) or 2 (lat/lon)\n");
        return 1;
    }

    if (grid)
    {
        retval =  locator2longlat(&lon, &lat, grid);

        if (retval == RIG_OK)
        {
            printf("Locator %s = Lat:%g, Lon:%g\n", grid, lat, lon);
        }
    }
    else
    {
        char locator[24];
        retval = longlat2locator(lon, lat, locator, 6);

        if (retval == RIG_OK)
        {
            printf("Locator lat:%g,lon:%g = %s\n", lat, lon, locator);
        }
    }

    return retval;

}
