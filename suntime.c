#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Calculate sun rise (if sunrise parameter != 0) or set time using day of the year, location on earth, and zenith angle
// Result stored in location by hour and min in UTC
// Return 0 on success, if the sun never rises returns 1, and -1 if sun never sets
int calc_suntime(int *hour, int *min, int sunrise, int yearday, double latitude, double longitude, double zenith) {
    // 1. first calculate the day of the year
    int N = yearday + 1;

    // 2. convert the longitude to hour value and calculate an approximate time
    double lngHour = longitude / 15;
    double t = sunrise ? N + ((6 - lngHour) / 24) : N + ((18 - lngHour) / 24);

    // 3. calculate the Sun's mean anomaly
    double M = (0.9856 * t) - 3.289;

    // 4. calculate the Sun's true longitude
    double L = M + (1.916 * sin((M_PI / 180) * M)) + (0.020 * sin(2 * (M_PI / 180) * M)) + 282.634;
    // NOTE: L potentially needs to be adjusted into the range [0,360) by adding/subtracting 360
    if (L < 0)
        L += 360;
    else if (L >= 360)
        L -= 360;

    // 5a. calculate the Sun's right ascension
    double RA = (180 / M_PI) * atan(0.91764 * tan((M_PI / 180) * L));
    // NOTE: RA potentially needs to be adjusted into the range [0,360) by adding/subtracting 360
    if (RA < 0)
        RA += 360;
    else if (RA >= 360)
        RA -= 360;

    // 5b. right ascension value needs to be in the same quadrant as L
    double Lquadrant = floor(L / 90) * 90;
    double RAquadrant = floor(RA / 90) * 90;
    RA = RA + (Lquadrant - RAquadrant);

    // 5c. right ascension value needs to be converted into hours
    RA = RA / 15;

    // 6. calculate the Sun's declination
    double sinDec = 0.39782 * sin((M_PI / 180) * L);
    double cosDec = cos(asin(sinDec));

    // 7a. calculate the Sun's local hour angle
    double cosH = (cos((M_PI / 180) * zenith) - (sinDec * sin((M_PI / 180) * latitude))) / (cosDec * cos((M_PI / 180) * latitude));

    if (cosH > 1)
        // the sun never rises on this location (on the specified date)
        return 1;

    if (cosH < -1)
        // the sun never sets on this location (on the specified date)
        return -1;

    // 7b. finish calculating H and convert into hours
    double H = (180 / M_PI) * acos(cosH);
    if (sunrise)
        H = 360 - H;
    H = H / 15;

    // 8. calculate local mean time of rising/setting
    double T = H + RA - (0.06571 * t) - 6.622;

    // 9. adjust back to UTC
    double UT = T - lngHour;
    // NOTE: UT potentially needs to be adjusted into the range [0,24) by adding/subtracting 24
    if (UT < 0)
        UT += 24;
    else if (UT >= 24)
        UT -= 24;

    double frac = modf(UT, &UT);

    *hour = UT;
    *min = floor(frac * 60);

    return 0;
}

int suntime(time_t *rises, time_t *sets, time_t t, int latitude[3], int longitude[3]) {
    struct tm tm;
    gmtime_r(&t, &tm);

    double y = (latitude[0] + 180) + (latitude[1] / 60.0) + (latitude[2] / 3600.0) - 180;
    double x = (longitude[0] + 180) + (longitude[1] / 60.0) + (longitude[2] / 3600.0) - 180;
    double z = 90 + (50 / 60.0);

    if (calc_suntime(&tm.tm_hour, &tm.tm_min, 1, tm.tm_yday, y, x, z))
        return 1;
    *rises = timegm(&tm);

    if (calc_suntime(&tm.tm_hour, &tm.tm_min, 0, tm.tm_yday, y, x, z))
        return 1;
    *sets = timegm(&tm);

    return 0;
}

// Latitude and longitude of the zone's principal location
// in ISO 6709 sign-degrees-minutes-seconds format,
// either +-DDMM+-DDDMM or +-DDMMSS+-DDDMMSS,
// first latitude (+ is north), then longitude (+ is east).
int tzparseloc(int y[3], int x[3], const char *s) {
    switch (strlen(s)) {
    case 11:
        y[2] = x[2] = 0;
        return sscanf(s, "%3d%2u%4d%2u", y + 0, y + 1, x + 0, x + 1) - 4;
    case 15:
        return sscanf(s, "%3d%2u%2u%4d%2u%2u", y + 0, y + 1, y + 2, x + 0, x + 1, x + 2) - 6;
    default:
        return -1;
    }
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: %s <offset in days> <location...>\n", argv[0]);
        return 1;
    }

    time_t t = time(NULL) + strtol(argv[1], NULL, 10) * (24 * 3600);

    for (int i = 2; i < argc; ++i) {
        int latitude[3], longitude[3];
        if (tzparseloc(latitude, longitude, argv[i])) {
            fprintf(stderr, "error: %s: bad location coords\n", argv[i]);
        } else {
            time_t sunrise, sunset;
            if (suntime(&sunrise, &sunset, t, latitude, longitude)) {
                fprintf(stderr, "error: %s: in this damned place sun today never rises or sets\n", argv[i]);
            } else {
                struct tm rtm, stm;
                localtime_r(&sunrise, &rtm);
                localtime_r(&sunset, &stm);
                printf("%04d-%02d-%02d\t%02d:%02d\t%02d:%02d\n", rtm.tm_year + 1900, rtm.tm_mon + 1, rtm.tm_mday, rtm.tm_hour, rtm.tm_min, stm.tm_hour, stm.tm_min);
            }
        }
    }

    return 0;
}
