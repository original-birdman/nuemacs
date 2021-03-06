/* combi.h
 * utf8proc Combining Class ranges for version 2.4.0.
 * Generated at Tue Jan 14 11:47:55 2020.
 *
 * ANNOTATE any hand-made alterations!!
 */
#ifndef COMBI_H
#define COMBI_H

static struct range_t {
    unicode_t start;
    unicode_t end;
} const combi_range[] = { /* THIS LIST MUST BE IN SORTED ORDER!! */
    {0x0300, 0x034e},
    {0x0350, 0x036f},
    {0x0483, 0x0487},
    {0x0591, 0x05bd},
    {0x05bf, 0x05bf},
    {0x05c1, 0x05c2},
    {0x05c4, 0x05c5},
    {0x05c7, 0x05c7},
    {0x0610, 0x061a},
    {0x064b, 0x065f},
    {0x0670, 0x0670},
    {0x06d6, 0x06dc},
    {0x06df, 0x06e4},
    {0x06e7, 0x06e8},
    {0x06ea, 0x06ed},
    {0x0711, 0x0711},
    {0x0730, 0x074a},
    {0x07eb, 0x07f3},
    {0x07fd, 0x07fd},
    {0x0816, 0x0819},
    {0x081b, 0x0823},
    {0x0825, 0x0827},
    {0x0829, 0x082d},
    {0x0859, 0x085b},
    {0x08d3, 0x08e1},
    {0x08e3, 0x08ff},
    {0x093c, 0x093c},
    {0x094d, 0x094d},
    {0x0951, 0x0954},
    {0x09bc, 0x09bc},
    {0x09cd, 0x09cd},
    {0x09fe, 0x09fe},
    {0x0a3c, 0x0a3c},
    {0x0a4d, 0x0a4d},
    {0x0abc, 0x0abc},
    {0x0acd, 0x0acd},
    {0x0b3c, 0x0b3c},
    {0x0b4d, 0x0b4d},
    {0x0bcd, 0x0bcd},
    {0x0c4d, 0x0c4d},
    {0x0c55, 0x0c56},
    {0x0cbc, 0x0cbc},
    {0x0ccd, 0x0ccd},
    {0x0d3b, 0x0d3c},
    {0x0d4d, 0x0d4d},
    {0x0dca, 0x0dca},
    {0x0e38, 0x0e3a},
    {0x0e48, 0x0e4b},
    {0x0eb8, 0x0eba},
    {0x0ec8, 0x0ecb},
    {0x0f18, 0x0f19},
    {0x0f35, 0x0f35},
    {0x0f37, 0x0f37},
    {0x0f39, 0x0f39},
    {0x0f71, 0x0f72},
    {0x0f74, 0x0f74},
    {0x0f7a, 0x0f7d},
    {0x0f80, 0x0f80},
    {0x0f82, 0x0f84},
    {0x0f86, 0x0f87},
    {0x0fc6, 0x0fc6},
    {0x1037, 0x1037},
    {0x1039, 0x103a},
    {0x108d, 0x108d},
    {0x135d, 0x135f},
    {0x1714, 0x1714},
    {0x1734, 0x1734},
    {0x17d2, 0x17d2},
    {0x17dd, 0x17dd},
    {0x18a9, 0x18a9},
    {0x1939, 0x193b},
    {0x1a17, 0x1a18},
    {0x1a60, 0x1a60},
    {0x1a75, 0x1a7c},
    {0x1a7f, 0x1a7f},
    {0x1ab0, 0x1abd},
    {0x1b34, 0x1b34},
    {0x1b44, 0x1b44},
    {0x1b6b, 0x1b73},
    {0x1baa, 0x1bab},
    {0x1be6, 0x1be6},
    {0x1bf2, 0x1bf3},
    {0x1c37, 0x1c37},
    {0x1cd0, 0x1cd2},
    {0x1cd4, 0x1ce0},
    {0x1ce2, 0x1ce8},
    {0x1ced, 0x1ced},
    {0x1cf4, 0x1cf4},
    {0x1cf8, 0x1cf9},
    {0x1dc0, 0x1df9},
    {0x1dfb, 0x1dff},
    {0x20d0, 0x20dc},
    {0x20e1, 0x20e1},
    {0x20e5, 0x20f0},
    {0x2cef, 0x2cf1},
    {0x2d7f, 0x2d7f},
    {0x2de0, 0x2dff},
    {0x302a, 0x302f},
    {0x3099, 0x309a},
    {0xa66f, 0xa66f},
    {0xa674, 0xa67d},
    {0xa69e, 0xa69f},
    {0xa6f0, 0xa6f1},
    {0xa806, 0xa806},
    {0xa8c4, 0xa8c4},
    {0xa8e0, 0xa8f1},
    {0xa92b, 0xa92d},
    {0xa953, 0xa953},
    {0xa9b3, 0xa9b3},
    {0xa9c0, 0xa9c0},
    {0xaab0, 0xaab0},
    {0xaab2, 0xaab4},
    {0xaab7, 0xaab8},
    {0xaabe, 0xaabf},
    {0xaac1, 0xaac1},
    {0xaaf6, 0xaaf6},
    {0xabed, 0xabed},
    {0xfb1e, 0xfb1e},
    {0xfe20, 0xfe2f},
    {0x101fd, 0x101fd},
    {0x102e0, 0x102e0},
    {0x10376, 0x1037a},
    {0x10a0d, 0x10a0d},
    {0x10a0f, 0x10a0f},
    {0x10a38, 0x10a3a},
    {0x10a3f, 0x10a3f},
    {0x10ae5, 0x10ae6},
    {0x10d24, 0x10d27},
    {0x10f46, 0x10f50},
    {0x11046, 0x11046},
    {0x1107f, 0x1107f},
    {0x110b9, 0x110ba},
    {0x11100, 0x11102},
    {0x11133, 0x11134},
    {0x11173, 0x11173},
    {0x111c0, 0x111c0},
    {0x111ca, 0x111ca},
    {0x11235, 0x11236},
    {0x112e9, 0x112ea},
    {0x1133b, 0x1133c},
    {0x1134d, 0x1134d},
    {0x11366, 0x1136c},
    {0x11370, 0x11374},
    {0x11442, 0x11442},
    {0x11446, 0x11446},
    {0x1145e, 0x1145e},
    {0x114c2, 0x114c3},
    {0x115bf, 0x115c0},
    {0x1163f, 0x1163f},
    {0x116b6, 0x116b7},
    {0x1172b, 0x1172b},
    {0x11839, 0x1183a},
    {0x119e0, 0x119e0},
    {0x11a34, 0x11a34},
    {0x11a47, 0x11a47},
    {0x11a99, 0x11a99},
    {0x11c3f, 0x11c3f},
    {0x11d42, 0x11d42},
    {0x11d44, 0x11d45},
    {0x11d97, 0x11d97},
    {0x16af0, 0x16af4},
    {0x16b30, 0x16b36},
    {0x1bc9e, 0x1bc9e},
    {0x1d165, 0x1d169},
    {0x1d16d, 0x1d172},
    {0x1d17b, 0x1d182},
    {0x1d185, 0x1d18b},
    {0x1d1aa, 0x1d1ad},
    {0x1d242, 0x1d244},
    {0x1e000, 0x1e006},
    {0x1e008, 0x1e018},
    {0x1e01b, 0x1e021},
    {0x1e023, 0x1e024},
    {0x1e026, 0x1e02a},
    {0x1e130, 0x1e136},
    {0x1e2ec, 0x1e2ef},
    {0x1e8d0, 0x1e8d6},
    {0x1e944, 0x1e94a},
    {UEM_NOCHAR, UEM_NOCHAR}    /* End of list marker */
};
#endif
