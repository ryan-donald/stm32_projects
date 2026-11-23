#include "nmea.h"

// parses "[-]123.456" as an integer times 10^decimals
// str_to_scaled_int("545.4", 3) -> 545400
static int32_t str_to_scaled_int(const char *s, uint32_t decimals)
{
    int32_t sign = 1;
    int32_t value = 0;

    if (*s == '-')
    {
        sign = -1;
        s++;
    }

    // integer part
    while (*s >= '0' && *s <= '9')
    {
        value = value * 10 + (*s++ - '0');
    }

    // fraction part
    if (*s == '.')
    {
        s++;
    }
    for (uint32_t i = 0; i < decimals; i++)
    {
        value *= 10;
        if (*s >= '0' && *s <= '9')
        {
            value += *s++ - '0';
        }
    }

    return sign * value;
}

// converts NMEA ddmm.mmmmm / dddmm.mmmmm and its hemisphere to signed 1e-7 degrees
static int32_t parse_degrees(const char *s, const char *hemi)
{
    int32_t value = str_to_scaled_int(s, 5); // turns ddmm.mmmmm into ddmmmmmmm (ddd for longitude)
    int32_t deg = value / 10000000; // extracts dd (ddd for longitude)
    int32_t min_e5 = value % 10000000; // extracts mmmmmmm

    // minutes from 1e-5 to 1e-7 units
    int32_t min_e7 = min_e5 * 100;

    // minutes to degrees and rounded to the nearest unit
    int32_t frac_e7 = (min_e7 + 30) / 60;

    // whole degrees to 1e-7 units plus the fraction
    int32_t result = deg * 10000000 + frac_e7;

    // south/west is negative
    return (*hemi == 'S' || *hemi == 'W') ? -result : result;
}

// parses the fields of a fix data frame into the fix
static void parse_fix_data(nmea_t *gnss, char **f)
{
    // parse time into ms
    int32_t t = str_to_scaled_int(f[1], 3);
    int32_t hh = t / 10000000;
    int32_t mm = (t / 100000) % 100;
    int32_t ss_ms = t % 100000;

    gnss->fix.time_ms = (uint32_t)((hh * 60 + mm) * 60000 + ss_ms);
    gnss->fix.lat = parse_degrees(f[2], f[3]);
    gnss->fix.lon = parse_degrees(f[4], f[5]);
    gnss->fix.quality = (uint8_t)str_to_scaled_int(f[6], 0);
    gnss->fix.satellites = (uint8_t)str_to_scaled_int(f[7], 0);
    gnss->fix.hdop = (uint32_t)str_to_scaled_int(f[8], 2);
    gnss->fix.alt_mm = str_to_scaled_int(f[9], 3);
    gnss->fix_count++;
}

// checks, splits and parses one data frame
static bool parse_line(nmea_t *gnss)
{
    char *line = gnss->line;
    uint32_t len = gnss->line_len;

    // shortest valid data frame is "$xxxxx*HH"
    if (len < 9 || line[0] != '$' || line[len - 3] != '*')
    {
        gnss->bad_count++;
        return false;
    }

    // checksum is the XOR of everything between '$' and '*'
    const char hex[] = "0123456789ABCDEF";
    uint8_t sum = 0;
    for (uint32_t i = 1; i < len - 3; i++)
    {
        sum ^= (uint8_t)line[i];
    }
    if (line[len - 2] != hex[sum >> 4] || line[len - 1] != hex[sum & 0x0F])
    {
        gnss->bad_count++;
        return false;
    }

    // split at commas in place. f[0] is the system + frame type
    char *f[NMEA_MAX_FIELDS];
    uint32_t count = 0;
    line[len - 3] = '\0';
    f[count++] = &line[1];
    for (char *p = &line[1]; *p != '\0' && count < NMEA_MAX_FIELDS; p++)
    {
        if (*p == ',')
        {
            *p = '\0';
            f[count++] = p + 1;
        }
    }

    // match any system (GP, GN, ...) followed by GGA
    if (count >= 10 && f[0][2] == 'G' && f[0][3] == 'G' && f[0][4] == 'A' && f[0][5] == '\0')
    {
        parse_fix_data(gnss, f);
        return true;
    }
    return false;
}

// reads all received bytes without blocking, returns true if fix was updated
bool nmea_poll(nmea_t *gnss)
{
    bool updated = false;

    while (uart_rx_ready(gnss->config->uart))
    {
        char c = uart_getc(gnss->config->uart);

        if (c == '\r' || c == '\n')
        {
            if (gnss->line_len > 0)
            {
                updated |= parse_line(gnss);
                gnss->line_len = 0;
            }
        }
        // the frame is too long, this will result in a failure in parsing
        else if (gnss->line_len < NMEA_LINE_SIZE)
        {
            gnss->line[gnss->line_len++] = c;
        }
    }

    return updated;
}
