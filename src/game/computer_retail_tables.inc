/* Source-authored COMPUTER.S terminal routing metadata.  Text/image payloads
 * remain ROM-owned; these records contain only offsets, handler identity and page counts. */
static const u32 pages_armtab[]={ 0x275e6u };
static const u32 pages_bodytab[]={ 0x279d4u };
static const u32 pages_brieftab[]={ 0x267e6u,0x2693cu,0x26a12u };
static const u32 pages_brigtab[]={ 0x265b4u,0x26676u,0x26734u,0x2677au };
static const u32 pages_com2tab[]={ 0x27f94u };
static const u32 pages_comptab[]={ 0x27ad8u,0x27b76u,0x27c78u,0x27d98u,0x27e64u,0x27f18u };
static const u32 pages_comtab[]={ 0x28666u };
static const u32 pages_flametab[]={ 0x279d4u };
static const u32 pages_greentab[]={ 0x26aeeu,0x26c20u,0x26d54u,0x26e96u };
static const u32 pages_libstab[]={ 0x27f94u,0x2804eu,0x281aeu,0x28216u,0x283c8u,0x28530u };
static const u32 pages_libtab[]={ 0x27f94u,0x2804eu,0x281aeu,0x28216u,0x28324u };
static const u32 pages_mstab[]={ 0x26f58u };
static const u32 pages_nlibtab[]={ 0x2896au,0x28a10u,0x28ac0u };
static const u32 pages_nlocktab[]={ 0x290eau };
static const u32 pages_optab[]={ 0x270eau,0x271e4u,0x27306u,0x2743cu,0x2753eu };
static const u32 pages_pettytab[]={ 0x28722u,0x28894u };
static const u32 pages_podtab[]={ 0x29272u };
static const u32 pages_pulsetab[]={ 0x277ecu };
static const u32 pages_securetab[]={ 0x2904cu };
static const u32 pages_slibtab[]={ 0x28b3cu,0x28ca0u,0x28d4au,0x28e1au };
static const u32 pages_smartab[]={ 0x2768au };
static const u32 pages_supplytab[]={ 0x26f58u,0x27008u,0x27092u };
static const u32 pages_tg1tab[]={ 0x27ad8u,0x27b76u,0x28ee4u };
static const u32 pages_tmtab[]={ 0x28fdcu };

static const AvpComputerContent content_libtab_access9={6u,pages_libstab,NULL,0u};
static const AvpComputerContent content_armtab={1u,pages_armtab,NULL,0u};
static const AvpComputerContent content_bodytab={1u,pages_bodytab,NULL,0u};
static const AvpComputerContent content_brieftab={3u,pages_brieftab,NULL,0u};
static const AvpComputerContent content_brigtab={4u,pages_brigtab,NULL,0u};
static const AvpComputerContent content_com2tab={1u,pages_com2tab,NULL,0u};
static const AvpComputerContent content_comptab={6u,pages_comptab,NULL,0u};
static const AvpComputerContent content_comtab={1u,pages_comtab,NULL,1u};
static const AvpComputerContent content_flametab={1u,pages_flametab,NULL,0u};
static const AvpComputerContent content_greentab={4u,pages_greentab,NULL,0u};
static const AvpComputerContent content_libtab={5u,pages_libtab,&content_libtab_access9,0u};
static const AvpComputerContent content_mstab={1u,pages_mstab,NULL,0u};
static const AvpComputerContent content_nlibtab={3u,pages_nlibtab,NULL,0u};
static const AvpComputerContent content_nlocktab={1u,pages_nlocktab,NULL,0u};
static const AvpComputerContent content_optab={5u,pages_optab,NULL,0u};
static const AvpComputerContent content_pettytab={2u,pages_pettytab,NULL,0u};
static const AvpComputerContent content_podtab={1u,pages_podtab,NULL,0u};
static const AvpComputerContent content_pulsetab={1u,pages_pulsetab,NULL,0u};
static const AvpComputerContent content_securetab={1u,pages_securetab,NULL,0u};
static const AvpComputerContent content_slibtab={4u,pages_slibtab,NULL,0u};
static const AvpComputerContent content_smartab={1u,pages_smartab,NULL,0u};
static const AvpComputerContent content_supplytab={3u,pages_supplytab,NULL,0u};
static const AvpComputerContent content_tg1tab={3u,pages_tg1tab,NULL,0u};
static const AvpComputerContent content_tmtab={1u,pages_tmtab,NULL,0u};

static const AvpComputerTerminal retail_level1_terminals[]={
    {1763u,AVP_COMP_GENERIC,&content_greentab},
    {1851u,AVP_COMP_GENERIC,&content_greentab},
    {2266u,AVP_COMP_GENERIC,&content_greentab},
    {2280u,AVP_COMP_GENERIC,&content_greentab},
    {2354u,AVP_COMP_GENERIC,&content_greentab},
    {2368u,AVP_COMP_GENERIC,&content_greentab},
    {2785u,AVP_COMP_GENERIC,&content_greentab},
    {2873u,AVP_COMP_GENERIC,&content_greentab},
};

static const AvpComputerTerminal retail_level2_terminals[]={
    {4200u,AVP_COMP_GENERIC,&content_com2tab},
    {5736u,AVP_COMP_GENERIC,&content_com2tab},
    {6299u,AVP_COMP_GENERIC,&content_comtab},
    {6331u,AVP_COMP_GENERIC,&content_comtab},
    {6802u,AVP_COMP_GENERIC,&content_comtab},
    {6803u,AVP_COMP_GENERIC,&content_comtab},
    {6816u,AVP_COMP_GENERIC,&content_comtab},
    {6834u,AVP_COMP_GENERIC,&content_comtab},
    {6848u,AVP_COMP_GENERIC,&content_comtab},
    {6851u,AVP_COMP_GENERIC,&content_comtab},
    {7306u,AVP_COMP_GENERIC,&content_comtab},
    {7320u,AVP_COMP_GENERIC,&content_comtab},
    {7321u,AVP_COMP_GENERIC,&content_comtab},
    {7353u,AVP_COMP_GENERIC,&content_comtab},
    {7354u,AVP_COMP_GENERIC,&content_comtab},
    {7368u,AVP_COMP_GENERIC,&content_comtab},
    {7825u,AVP_COMP_GENERIC,&content_comtab},
    {7873u,AVP_COMP_GENERIC,&content_comtab},
    {14497u,AVP_COMP_GENERIC,&content_libtab},
    {16571u,AVP_COMP_GENERIC,&content_libtab},
    {16656u,AVP_COMP_GENERIC,&content_comptab},
    {16657u,AVP_COMP_GENERIC,&content_comptab},
    {16665u,AVP_COMP_GENERIC,&content_comptab},
    {16666u,AVP_COMP_GENERIC,&content_comptab},
    {16712u,AVP_COMP_GENERIC,&content_comptab},
    {16713u,AVP_COMP_GENERIC,&content_comptab},
    {16721u,AVP_COMP_GENERIC,&content_comptab},
    {16722u,AVP_COMP_GENERIC,&content_comptab},
    {17168u,AVP_COMP_GENERIC,&content_comptab},
    {17171u,AVP_COMP_GENERIC,&content_comptab},
    {17234u,AVP_COMP_GENERIC,&content_comptab},
    {17235u,AVP_COMP_GENERIC,&content_comptab},
    {6466u,AVP_COMP_ARMOURY,&content_armtab},
    {7496u,AVP_COMP_ARMOURY,&content_armtab},
    {7506u,AVP_COMP_ARMOURY,&content_smartab},
    {8001u,AVP_COMP_ARMOURY,&content_armtab},
    {8025u,AVP_COMP_ARMOURY,&content_smartab},
    {8459u,AVP_COMP_ARMOURY,&content_bodytab},
    {8976u,AVP_COMP_ARMOURY,&content_bodytab},
    {9488u,AVP_COMP_ARMOURY,&content_bodytab},
    {9993u,AVP_COMP_ARMOURY,&content_bodytab},
    {10587u,AVP_COMP_ARMOURY,&content_pulsetab},
    {11090u,AVP_COMP_ARMOURY,&content_pulsetab},
    {11609u,AVP_COMP_ARMOURY,&content_pulsetab},
    {12043u,AVP_COMP_ARMOURY,&content_flametab},
    {12560u,AVP_COMP_ARMOURY,&content_flametab},
    {13072u,AVP_COMP_ARMOURY,&content_flametab},
    {13577u,AVP_COMP_ARMOURY,&content_flametab},
    {14147u,AVP_COMP_ARMOURY,&content_flametab},
    {14171u,AVP_COMP_ARMOURY,&content_flametab},
    {14664u,AVP_COMP_ARMOURY,&content_flametab},
    {14674u,AVP_COMP_ARMOURY,&content_flametab},
    {15682u,AVP_COMP_ARMOURY,&content_armtab},
    {4113u,AVP_COMP_MEDICAL,&content_supplytab},
    {7730u,AVP_COMP_MEDICAL,&content_optab},
    {9778u,AVP_COMP_MEDICAL,&content_optab},
    {14363u,AVP_COMP_MEDICAL,&content_supplytab},
};

static const AvpComputerTerminal retail_level3_terminals[]={
    {2755u,AVP_COMP_GENERIC,&content_nlibtab},
    {3258u,AVP_COMP_GENERIC,&content_nlibtab},
    {5264u,AVP_COMP_GENERIC,&content_slibtab},
    {5769u,AVP_COMP_GENERIC,&content_slibtab},
    {6499u,AVP_COMP_GENERIC,&content_brigtab},
    {7002u,AVP_COMP_GENERIC,&content_brigtab},
    {7521u,AVP_COMP_GENERIC,&content_brigtab},
    {17707u,AVP_COMP_GENERIC,&content_brieftab},
    {17739u,AVP_COMP_GENERIC,&content_brieftab},
    {18210u,AVP_COMP_GENERIC,&content_brieftab},
    {18224u,AVP_COMP_GENERIC,&content_brieftab},
    {18242u,AVP_COMP_GENERIC,&content_brieftab},
    {18256u,AVP_COMP_GENERIC,&content_brieftab},
    {20273u,AVP_COMP_GENERIC,&content_pettytab},
    {20274u,AVP_COMP_GENERIC,&content_pettytab},
};

static const AvpComputerTerminal retail_level4_terminals[]={
    {2064u,AVP_COMP_GENERIC,&content_mstab},
    {3931u,AVP_COMP_GENERIC,&content_tmtab},
    {4434u,AVP_COMP_GENERIC,&content_tmtab},
    {4448u,AVP_COMP_GENERIC,&content_tmtab},
    {4953u,AVP_COMP_GENERIC,&content_tmtab},
    {5787u,AVP_COMP_GENERIC,&content_pulsetab},
    {5795u,AVP_COMP_GENERIC,&content_pulsetab},
    {15243u,AVP_COMP_GENERIC,&content_tmtab},
    {15746u,AVP_COMP_GENERIC,&content_tmtab},
    {15760u,AVP_COMP_GENERIC,&content_tmtab},
    {16265u,AVP_COMP_GENERIC,&content_tmtab},
    {18216u,AVP_COMP_GENERIC,&content_pulsetab},
    {18497u,AVP_COMP_GENERIC,&content_securetab},
    {18721u,AVP_COMP_GENERIC,&content_pulsetab},
    {9008u,AVP_COMP_ARMOURY,&content_bodytab},
    {9048u,AVP_COMP_ARMOURY,&content_pulsetab},
    {9513u,AVP_COMP_ARMOURY,&content_bodytab},
    {9553u,AVP_COMP_ARMOURY,&content_pulsetab},
    {11129u,AVP_COMP_ARMOURY,&content_smartab},
    {11130u,AVP_COMP_ARMOURY,&content_smartab},
    {11131u,AVP_COMP_ARMOURY,&content_smartab},
    {12587u,AVP_COMP_ARMOURY,&content_flametab},
    {12627u,AVP_COMP_ARMOURY,&content_flametab},
    {13104u,AVP_COMP_ARMOURY,&content_flametab},
    {13144u,AVP_COMP_ARMOURY,&content_flametab},
    {14049u,AVP_COMP_ARMOURY,&content_tg1tab},
    {14057u,AVP_COMP_ARMOURY,&content_tg1tab},
    {20139u,AVP_COMP_ARMOURY,&content_tg1tab},
    {20642u,AVP_COMP_ARMOURY,&content_tg1tab},
    {20656u,AVP_COMP_ARMOURY,&content_tg1tab},
    {21161u,AVP_COMP_ARMOURY,&content_tg1tab},
    {4178u,AVP_COMP_MEDICAL,&content_optab},
    {4184u,AVP_COMP_MEDICAL,&content_optab},
    {6226u,AVP_COMP_MEDICAL,&content_optab},
    {6232u,AVP_COMP_MEDICAL,&content_optab},
    {7762u,AVP_COMP_MEDICAL,&content_optab},
    {7768u,AVP_COMP_MEDICAL,&content_optab},
    {12873u,AVP_COMP_MEDICAL,&content_mstab},
};

static const AvpComputerTerminal retail_level5_terminals[]={
    {1763u,AVP_COMP_GENERIC,&content_nlocktab},
    {1851u,AVP_COMP_GENERIC,&content_nlocktab},
    {2266u,AVP_COMP_GENERIC,&content_nlocktab},
    {2280u,AVP_COMP_GENERIC,&content_nlocktab},
    {2354u,AVP_COMP_GENERIC,&content_nlocktab},
    {2368u,AVP_COMP_GENERIC,&content_nlocktab},
    {2785u,AVP_COMP_GENERIC,&content_nlocktab},
    {2873u,AVP_COMP_GENERIC,&content_nlocktab},
    {27275u,AVP_COMP_POD,&content_podtab},
    {27272u,AVP_COMP_POD,&content_podtab},
    {27273u,AVP_COMP_POD,&content_podtab},
};

