#include <Arduino.h>
#include "decodeutf8.h"
#include "gfxlatin2.h"

// Define the macro to flag as unmapped those Latin 1 characters
// that have been replaced with Latin 9 characters.

//#define INVALIDATE_OVERWRITTEN_LATIN_1_CHARS

uint16_t recode(uint8_t b)
{

    uint16_t ucs2 = decodeUTF8(b);

    if (ucs2 > 0x7F)
    {
#ifdef INVALIDATE_OVERWRITTEN_LATIN_1_CHARS
        if (0xA4 <= ucs2 && ucs2 <= 0xBE)
        {
            switch (ucs2)
            {
            case 0xa4:
            case 0xa6:
            case 0xa8:
            case 0xb4:
            case 0xb8:
            case 0xbc:
            case 0xbd:
            case 0xbe:
                return (showUnmapped) ? 0x7F : 0xFFFF;
            }
        }
#endif
        switch (ucs2)
        {

        //a0
        case 0x0104:
            return 0xa1;
            break; // Ą
        case 0x02D8:
            return 0xa2;
            break; // ˘
        case 0x0141:
            return 0xa3;
            break; // Ł
        case 0x013D:
            return 0xa5;
            break; // Ľ
        case 0x015A:
            return 0xa6;
            break; // Ś

        case 0x0160:
            return 0xa9;
            break; // Š
        case 0x015E:
            return 0xaa;
            break; // Ş
        case 0x0164:
            return 0xab;
            break; // Ť
        case 0x0179:
            return 0xac;
            break; // Ź
        case 0x017D:
            return 0xae;
            break; // Ž
        case 0x017B:
            return 0xaf;
            break; // Ż

        //b0
        case 0x0105:
            return 0xb1;
            break; // ą
        case 0x02DB:
            return 0xb2;
            break; // ˛
        case 0x0142:
            return 0xb3;
            break; // ł
        case 0x013E:
            return 0xb5;
            break; // ľ
        case 0x015B:
            return 0xb6;
            break; // ś
        case 0x02C7:
            return 0xb7;
            break; // ˇ

        case 0x0161:
            return 0xb9;
            break; // š
        case 0x015F:
            return 0xba;
            break; // ş
        case 0x0165:
            return 0xbb;
            break; // ť
        case 0x017A:
            return 0xbc;
            break; // ź
        case 0x02DD:
            return 0xbd;
            break; // ˝
        case 0x017E:
            return 0xbe;
            break; // ž
        case 0x017C:
            return 0xbf;
            break; // ż

            // c0
        case 0x0154:
            return 0xc0;
            break; // Ŕ
        case 0x0102:
            return 0xc3;
            break; // Ă
        case 0x0139:
            return 0xc5;
            break; // Ĺ
        case 0x0106:
            return 0xc6;
            break; // Ć

        case 0x010C:
            return 0xc8;
            break; // Č
        case 0x0118:
            return 0xca;
            break; // Ę
        case 0x011A:
            return 0xcc;
            break; // Ě
        case 0x00DF:
            return 0xcf;
            break; // ß
        case 0x1E9E:
            return 0xcf;
            break; // ẞ
        case 0x010E:
            return 0xdf;
            break; // Ď

            // d0
        case 0x0110:
            return 0xd0;
            break; // Đ
        case 0x0143:
            return 0xd1;
            break; // Ń
        case 0x0147:
            return 0xd2;
            break; // Ň
        case 0x0150:
            return 0xd5;
            break; // Ő

        case 0x0158:
            return 0xd8;
            break; // Ř
        case 0x016E:
            return 0xd9;
            break; // Ů
        case 0x0170:
            return 0xdb;
            break; // Ű
        case 0x0162:
            return 0xde;
            break; // Ţ

            //e0
        case 0x0155:
            return 0xe0;
            break; // ŕ
        case 0x0103:
            return 0xe3;
            break; // ă
        case 0x013A:
            return 0xe5;
            break; // ĺ
        case 0x0107:
            return 0xe6;
            break; // ć

        case 0x010D:
            return 0xe8;
            break; // č
        case 0x0119:
            return 0xea;
            break; // ę
        case 0x011B:
            return 0xec;
            break; // ě
        case 0x010F:
            return 0xef;
            break; // ď

            // f0
        case 0x0111:
            return 0xf0;
            break; // đ
        case 0x0144:
            return 0xf1;
            break; // ń
        case 0x0148:
            return 0xf2;
            break; // ň
        case 0x0151:
            return 0xf5;
            break; // ő

        case 0x0159:
            return 0xf8;
            break; // ř
        case 0x016F:
            return 0xf9;
            break; // ů
        case 0x0171:
            return 0xfb;
            break; // ű
        case 0x0163:
            return 0xfe;
            break; // ţ
        case 0x02D9:
            return 0xff;
            break; // ˙
        }
    }
    return ucs2;
}

// Convert String object from UTF8 string to extended ASCII
String utf8tocp(String s)
{
    String r = "";
    uint16_t ucs2;
    resetUTF8decoder();
    for (int i = 0; i < s.length(); i++)
    {
        ucs2 = recode(s.charAt(i));

        //dbg:: Serial.printf("s[%d]=0x%02x -> 0x%04x\n", i, (int) s.charAt(i), ucs2);

        if (0x20 <= ucs2 && ucs2 <= 0x7F)
            r += (char)ucs2;
        else if (0xA0 <= ucs2 && ucs2 <= 0xFF)
            r += (char)(ucs2 - 32);
        else if (showUnmapped && 0xFF < ucs2 && ucs2 < 0xFFFF)
            r += (char)0x7F;
    }
    return r;
}


// In place conversion of a UTF8 string to extended ASCII string (ASCII is shorter!)
void utf8tocp(char* s)
{
    int k = 0;
    uint16_t ucs2;
    resetUTF8decoder();
    for (int i = 0; i < strlen(s); i++)
    {
        ucs2 = recode(s[i]);

        //D/ Serial.printf("s[%d]=0x%02x -> 0x%04x\n", i, s[i], ucs2 );

        if (0x20 <= ucs2 && ucs2 <= 0x7F)
        {
            s[k++] = (char)ucs2;
            //D/Serial.printf("  > s[%d] = %02x (<7f)\n", k-1, s[k-1] );
        }
        else if (0xA0 <= ucs2 && ucs2 <= 0xFF)
        {
            s[k++] = (char)(ucs2 - 32);
            //D/Serial.printf("  > s[%d] = %02x (a0-ff)\n", k-1, s[k-1] );
        }
        else if (showUnmapped && 0xFF < ucs2 && ucs2 < 0xFFFF)
        {
            s[k++] = (char)127;
            //D/Serial.printf("  > s[%d] = %02x (x)\n", k-1, s[k-1] );
        }
    }
    s[k] = 0;
}
