#pragma once

#include <stdint.h>

#define SEG_A1_START    0
#define SEG_A1_END     92
#define SEG_A2_START   93
#define SEG_A2_END    185
#define SEG_B_START   186
#define SEG_B_END     367
#define SEG_C_START  1285
#define SEG_C_END    1465
#define SEG_D1_START 1099
#define SEG_D1_END   1191
#define SEG_D2_START 1192
#define SEG_D2_END   1284
#define SEG_E_START  1836
#define SEG_E_END    2017
#define SEG_F_START   735
#define SEG_F_END     916
#define SEG_G1_START  917
#define SEG_G1_END   1007
#define SEG_G2_START 1008
#define SEG_G2_END   1098
#define SEG_H_START   478
#define SEG_H_END     633
#define SEG_I_START  1567
#define SEG_I_END    1722
#define SEG_J_START   634
#define SEG_J_END     734
#define SEG_K_START   368
#define SEG_K_END     477
#define SEG_L_START  1466
#define SEG_L_END    1566
#define SEG_M_START  1723
#define SEG_M_END    1835
#define SEG_DP_START 2018
#define SEG_DP_END   2061

#define MAPPING_LENGTH 2062
static const uint16_t LED_TO_BITMAP_MAPPING[MAPPING_LENGTH] = {
    11538,
    11235,
    10932,
    10326,
    10023,
    9720,
    9114,
    8811,
    8508,
    7902,
    7599,
    7296,
    6993,
    6387,
    6081,
    6687,
    6990,
    7293,
    7596,
    8202,
    8505,
    8808,
    9414,
    9717,
    10020,
    10626,
    10929,
    11232,
    11535,
    11532,
    11229,
    10926,
    10623,
    10017,
    9714,
    9411,
    8805,
    8502,
    8199,
    7593,
    7290,
    6987,
    6684,
    6078,
    5775,
    5466,
    5769,
    6072,
    6678,
    6981,
    7284,
    7890,
    8193,
    8496,
    8799,
    9405,
    9708,
    10011,
    10617,
    10920,
    11223,
    11829,
    11826,
    11523,
    10917,
    10614,
    10311,
    9705,
    9402,
    9099,
    8493,
    8190,
    7887,
    7584,
    6978,
    6675,
    6372,
    5766,
    6369,
    6672,
    6975,
    7581,
    7884,
    8187,
    8490,
    9096,
    9399,
    10005,
    10308,
    10611,
    10914,
    11520,
    11823,
    12750,
    13053,
    13659,
    13962,
    14265,
    14871,
    15174,
    15477,
    15780,
    16386,
    16689,
    16992,
    17598,
    17901,
    18201,
    17898,
    17595,
    16989,
    16686,
    16383,
    15777,
    15474,
    15171,
    14565,
    14262,
    13959,
    13656,
    13050,
    12747,
    12744,
    13047,
    13653,
    13956,
    14259,
    14865,
    15168,
    15471,
    16077,
    16380,
    16683,
    16986,
    17592,
    17895,
    18198,
    18804,
    19101,
    18798,
    18495,
    17892,
    17589,
    17286,
    16677,
    16377,
    16074,
    15771,
    15165,
    14862,
    14559,
    13953,
    13650,
    13347,
    12741,
    13038,
    13341,
    13644,
    14250,
    14553,
    14856,
    15159,
    15765,
    16068,
    16371,
    16977,
    17280,
    17583,
    18189,
    18492,
    18795,
    18489,
    18186,
    17580,
    17277,
    16974,
    16368,
    16065,
    15762,
    15156,
    14853,
    14550,
    14247,
    13641,
    13338,
    13035,
    20631,
    20634,
    20637,
    20340,
    20343,
    20346,
    20349,
    20355,
    20358,
    20361,
    20061,
    20067,
    20070,
    20073,
    20079,
    20082,
    20085,
    19788,
    19791,
    19794,
    19797,
    19803,
    19503,
    19506,
    19509,
    19515,
    19518,
    19218,
    19224,
    19227,
    18927,
    18924,
    18921,
    18915,
    18912,
    19212,
    19209,
    19203,
    19200,
    19197,
    19194,
    19491,
    19488,
    19485,
    19479,
    19476,
    19473,
    19773,
    19767,
    19764,
    19761,
    19755,
    20055,
    20052,
    20046,
    20043,
    20040,
    20037,
    20334,
    20331,
    20328,
    20322,
    20019,
    20022,
    19722,
    19728,
    19731,
    19734,
    19737,
    19743,
    19443,
    19446,
    19452,
    19455,
    19458,
    19464,
    19164,
    19167,
    19170,
    19176,
    19179,
    19182,
    18885,
    18888,
    18891,
    18894,
    18900,
    18600,
    18603,
    18609,
    18612,
    18615,
    18618,
    18321,
    18324,
    18018,
    18012,
    18009,
    18309,
    18306,
    18300,
    18297,
    18294,
    18288,
    18588,
    18585,
    18582,
    18576,
    18573,
    18570,
    18870,
    18864,
    18861,
    18858,
    18852,
    19152,
    19149,
    19143,
    19140,
    19137,
    19134,
    19431,
    19428,
    19425,
    19419,
    19416,
    18816,
    18819,
    18822,
    18828,
    18831,
    18834,
    18534,
    18540,
    18543,
    18546,
    18552,
    18555,
    18255,
    18261,
    18264,
    18267,
    18270,
    18276,
    17976,
    17979,
    17982,
    17988,
    17991,
    17994,
    17697,
    17700,
    17703,
    17706,
    17712,
    17406,
    17400,
    17397,
    17394,
    17388,
    17688,
    17685,
    17682,
    17676,
    17673,
    17670,
    17667,
    17964,
    17961,
    17958,
    17952,
    17949,
    18249,
    18246,
    18240,
    18237,
    18234,
    18228,
    18528,
    18525,
    18522,
    18516,
    17610,
    17310,
    17010,
    16710,
    16713,
    16413,
    16113,
    15813,
    15513,
    15516,
    15219,
    14919,
    14619,
    14622,
    14322,
    14022,
    13722,
    13725,
    13425,
    13131,
    13431,
    13731,
    14031,
    14331,
    14328,
    14628,
    14928,
    15225,
    15222,
    15522,
    15822,
    16122,
    16119,
    16419,
    16719,
    17019,
    17016,
    17316,
    17022,
    17025,
    16728,
    16428,
    16128,
    16131,
    15831,
    15531,
    15231,
    15234,
    14934,
    14634,
    14334,
    14337,
    14037,
    13737,
    13437,
    13137,
    13146,
    13446,
    13746,
    14046,
    14040,
    14343,
    14643,
    14940,
    14937,
    15237,
    15537,
    15837,
    15834,
    16134,
    16434,
    16734,
    16731,
    17031,
    17040,
    16740,
    16440,
    16443,
    16143,
    15843,
    15543,
    15546,
    15246,
    14946,
    14646,
    14649,
    14349,
    14049,
    13749,
    13449,
    13452,
    13152,
    13161,
    13158,
    13458,
    13758,
    14058,
    14055,
    14355,
    14652,
    14952,
    14949,
    15252,
    15552,
    15849,
    16149,
    16146,
    16446,
    16746,
    17046,
    13065,
    13068,
    13071,
    12774,
    12777,
    12780,
    12783,
    12789,
    12792,
    12492,
    12498,
    12501,
    12504,
    12510,
    12513,
    12213,
    12216,
    12222,
    12225,
    12228,
    12231,
    12237,
    11937,
    11940,
    11946,
    11949,
    11646,
    11643,
    11637,
    11634,
    11631,
    11931,
    11925,
    11922,
    11919,
    11913,
    11910,
    11907,
    12207,
    12201,
    12198,
    12195,
    12189,
    12186,
    12183,
    12483,
    12477,
    12474,
    12471,
    12465,
    12765,
    12762,
    12156,
    12159,
    12162,
    12168,
    12171,
    12174,
    11874,
    11880,
    11883,
    11886,
    11892,
    11895,
    11595,
    11601,
    11604,
    11607,
    11610,
    11616,
    11316,
    11319,
    11322,
    11328,
    11331,
    11334,
    11340,
    11040,
    10737,
    10734,
    10728,
    11028,
    11025,
    11019,
    11016,
    11013,
    11010,
    11004,
    11304,
    11301,
    11298,
    11292,
    11289,
    11286,
    11586,
    11580,
    11577,
    11574,
    11568,
    11565,
    11865,
    11859,
    11856,
    11853,
    11550,
    11553,
    11253,
    11259,
    11262,
    11265,
    11271,
    11274,
    10974,
    10977,
    10983,
    10986,
    10989,
    10995,
    10998,
    10698,
    10701,
    10707,
    10710,
    10713,
    10719,
    10419,
    10422,
    10425,
    10431,
    10434,
    9828,
    10128,
    10122,
    10119,
    10116,
    10110,
    10107,
    10407,
    10404,
    10398,
    10395,
    10392,
    10389,
    10383,
    10683,
    10680,
    10677,
    10671,
    10668,
    10665,
    10659,
    10959,
    10956,
    10950,
    10947,
    10944,
    6702,
    6705,
    7011,
    7017,
    7323,
    7629,
    7632,
    7941,
    7944,
    8250,
    8253,
    8559,
    8562,
    8868,
    9177,
    9180,
    9486,
    9192,
    9189,
    8883,
    8877,
    8571,
    8568,
    8262,
    8259,
    7953,
    7644,
    7641,
    7335,
    7332,
    7026,
    7020,
    6714,
    6711,
    6417,
    6723,
    6726,
    7035,
    7038,
    7344,
    7347,
    7653,
    7959,
    7962,
    8271,
    8274,
    8580,
    8583,
    8889,
    8892,
    9201,
    9207,
    8901,
    8898,
    8592,
    8286,
    8283,
    7977,
    7971,
    7665,
    7662,
    7356,
    7353,
    7047,
    6738,
    6735,
    6429,
    6426,
    6132,
    6438,
    6744,
    6747,
    7053,
    7056,
    7365,
    7368,
    7674,
    7980,
    7983,
    8289,
    8292,
    8601,
    8604,
    8910,
    9216,
    8919,
    8613,
    8607,
    8301,
    8298,
    7992,
    7686,
    7683,
    7377,
    7371,
    7065,
    6759,
    6756,
    6450,
    6447,
    6141,
    5487,
    5490,
    5496,
    5499,
    5502,
    5505,
    5511,
    5514,
    5214,
    5220,
    5223,
    5226,
    5232,
    5235,
    5238,
    4938,
    4944,
    4947,
    4950,
    4953,
    4959,
    4962,
    4662,
    4668,
    4671,
    4674,
    4677,
    4077,
    4071,
    4371,
    4368,
    4365,
    4359,
    4356,
    4353,
    4347,
    4647,
    4644,
    4641,
    4635,
    4632,
    4629,
    4929,
    4923,
    4920,
    4917,
    4914,
    4908,
    4905,
    4902,
    5199,
    5196,
    5193,
    5187,
    5184,
    5181,
    4872,
    4878,
    4881,
    4884,
    4887,
    4590,
    4593,
    4596,
    4602,
    4605,
    4305,
    4311,
    4314,
    4317,
    4320,
    4326,
    4329,
    4332,
    4335,
    4038,
    4041,
    4044,
    4050,
    4053,
    4056,
    4059,
    3762,
    3765,
    3768,
    3771,
    3777,
    3174,
    3171,
    3165,
    3465,
    3462,
    3459,
    3453,
    3450,
    3447,
    3444,
    3741,
    3738,
    3735,
    3729,
    3726,
    3723,
    3720,
    4017,
    4014,
    4011,
    4005,
    4002,
    3999,
    3996,
    4293,
    4290,
    4287,
    4281,
    4278,
    4275,
    4272,
    4569,
    4566,
    3963,
    3966,
    3969,
    3975,
    3978,
    3981,
    3681,
    3687,
    3690,
    3693,
    3699,
    3702,
    3705,
    3711,
    3411,
    3414,
    3417,
    3420,
    3426,
    3429,
    3432,
    3135,
    3138,
    3141,
    3144,
    3150,
    3153,
    3156,
    2856,
    2862,
    2865,
    2868,
    2562,
    2559,
    2556,
    2550,
    2850,
    2847,
    2841,
    2838,
    2835,
    2832,
    2826,
    2823,
    3123,
    3120,
    3114,
    3111,
    3108,
    3102,
    3099,
    3399,
    3396,
    3390,
    3387,
    3384,
    3378,
    3375,
    3675,
    3672,
    3666,
    3663,
    10161,
    9858,
    9252,
    8949,
    8646,
    8040,
    7737,
    7434,
    7131,
    6525,
    6222,
    5919,
    5313,
    5010,
    4701,
    5004,
    5610,
    5913,
    6216,
    6519,
    7125,
    7428,
    7731,
    8337,
    8643,
    8943,
    9246,
    9852,
    10155,
    10152,
    9849,
    9546,
    8940,
    8637,
    8334,
    8031,
    7425,
    7122,
    6819,
    6213,
    5910,
    5607,
    5304,
    4698,
    4395,
    4092,
    4392,
    4695,
    5301,
    5604,
    5907,
    6210,
    6816,
    7119,
    7422,
    8028,
    8331,
    8634,
    8937,
    9543,
    9846,
    10149,
    10143,
    9840,
    9537,
    8931,
    8628,
    8325,
    8022,
    7416,
    7113,
    6810,
    6507,
    5901,
    5598,
    5295,
    4992,
    5292,
    5595,
    6201,
    6504,
    6807,
    7110,
    7716,
    8019,
    8322,
    8928,
    9231,
    9534,
    9837,
    10443,
    11070,
    11676,
    11979,
    12282,
    12585,
    13191,
    13494,
    13797,
    14403,
    14706,
    15009,
    15312,
    15918,
    16221,
    16821,
    16215,
    15912,
    15609,
    15003,
    14700,
    14397,
    14094,
    13488,
    13185,
    12579,
    12276,
    11973,
    11670,
    11064,
    11364,
    11667,
    11970,
    12576,
    12879,
    13182,
    13788,
    14091,
    14394,
    14697,
    15303,
    15606,
    15909,
    16515,
    16818,
    17121,
    17421,
    17118,
    16815,
    16209,
    15906,
    15603,
    14997,
    14694,
    14391,
    14088,
    13482,
    13179,
    12876,
    12270,
    11967,
    11664,
    11361,
    11355,
    11658,
    12264,
    12567,
    12870,
    13173,
    13779,
    14082,
    14385,
    14991,
    15294,
    15597,
    15900,
    16506,
    16809,
    16503,
    16200,
    15594,
    15291,
    14988,
    14382,
    14079,
    13776,
    13473,
    12867,
    12564,
    12261,
    11655,
    11352,
    8760,
    8457,
    8154,
    7548,
    7245,
    6942,
    6336,
    6033,
    5730,
    5124,
    4821,
    4518,
    4215,
    3609,
    3309,
    3612,
    3915,
    4521,
    4824,
    5127,
    5733,
    6036,
    6339,
    6642,
    7251,
    7551,
    7854,
    8460,
    8763,
    8769,
    8163,
    7860,
    7557,
    7254,
    6648,
    6345,
    6042,
    5436,
    5133,
    4830,
    4527,
    3921,
    3618,
    3315,
    2709,
    2409,
    2712,
    3318,
    3621,
    3924,
    4227,
    4833,
    5136,
    5439,
    6045,
    6348,
    6651,
    6954,
    7560,
    7863,
    8166,
    8772,
    8472,
    8169,
    7866,
    7563,
    6957,
    6654,
    6351,
    5745,
    5442,
    5139,
    4533,
    4230,
    3927,
    3624,
    3018,
    2715,
    3024,
    3327,
    3933,
    4233,
    4536,
    5145,
    5448,
    5751,
    6357,
    6660,
    6960,
    7266,
    7872,
    8175,
    8478,
    9972,
    10275,
    10578,
    11184,
    11487,
    11790,
    12396,
    12699,
    13002,
    13305,
    13911,
    14214,
    14517,
    15123,
    15429,
    14823,
    14520,
    14217,
    13914,
    13308,
    13005,
    12702,
    12096,
    11793,
    11490,
    10884,
    10581,
    10278,
    9975,
    9678,
    10284,
    10587,
    10890,
    11496,
    11799,
    12102,
    12405,
    13011,
    13314,
    13617,
    14223,
    14526,
    14829,
    15132,
    15738,
    16044,
    15741,
    15135,
    14832,
    14529,
    14226,
    13620,
    13317,
    13014,
    12408,
    12105,
    11802,
    11499,
    10893,
    10590,
    10287,
    9681,
    9684,
    10290,
    10593,
    10896,
    11199,
    11805,
    12108,
    12411,
    13017,
    13320,
    13623,
    13926,
    14532,
    14835,
    15138,
    15744,
    15141,
    14838,
    14232,
    13932,
    13626,
    13323,
    12717,
    12414,
    12114,
    11508,
    11205,
    10899,
    10596,
    9993,
    9690,
    17847,
    17844,
    17841,
    17838,
    17832,
    17829,
    18129,
    18123,
    18120,
    18117,
    18111,
    18108,
    18105,
    18405,
    18402,
    18396,
    18393,
    18390,
    18384,
    18684,
    18681,
    18678,
    18672,
    18669,
    18666,
    18663,
    18960,
    18957,
    18954,
    18951,
    18645,
    18648,
    18651,
    18657,
    18357,
    18360,
    18366,
    18369,
    18372,
    18375,
    18378,
    18081,
    18084,
    18087,
    18093,
    18096,
    18099,
    17799,
    17805,
    17808,
    17811,
    17814,
    17820,
    17823,
    17523,
    17526,
    17532,
    17535,
    17538,
    17544,
    17244,
    16947,
    16941,
    16938,
    16935,
    16929,
    17229,
    17226,
    17220,
    17217,
    17214,
    17211,
    17205,
    17505,
    17502,
    17499,
    17493,
    17490,
    17487,
    17481,
    17781,
    17778,
    17775,
    17769,
    17766,
    17763,
    17760,
    18057,
    18054,
    18051,
    18048,
    18345,
    18342,
    18339,
    17736,
    17739,
    17745,
    17748,
    17448,
    17451,
    17457,
    17460,
    17463,
    17466,
    17472,
    17172,
    17175,
    17178,
    17184,
    17187,
    17190,
    17196,
    16896,
    16899,
    16902,
    16908,
    16911,
    16914,
    16917,
    16620,
    16623,
    16626,
    16632,
    16635,
    16638,
    16029,
    16329,
    16323,
    16320,
    16317,
    16311,
    16308,
    16305,
    16602,
    16599,
    16596,
    16593,
    16587,
    16584,
    16581,
    16881,
    16875,
    16872,
    16869,
    16866,
    16860,
    17160,
    17157,
    17154,
    17148,
    17145,
    17142,
    17139,
    17436,
    16836,
    16839,
    16842,
    16845,
    16548,
    16551,
    16554,
    16557,
    16563,
    16566,
    16569,
    16269,
    16275,
    16278,
    16281,
    16284,
    16290,
    15990,
    15993,
    15999,
    16002,
    16005,
    16008,
    16014,
    16017,
    15717,
    15723,
    14808,
    14805,
    14499,
    14193,
    14190,
    13881,
    13878,
    13572,
    13569,
    13263,
    12957,
    12951,
    12645,
    12642,
    12336,
    12333,
    12027,
    12321,
    12324,
    12630,
    12633,
    12939,
    12945,
    13251,
    13254,
    13560,
    13563,
    13869,
    14175,
    14181,
    14487,
    14490,
    14796,
    14802,
    15096,
    14787,
    14784,
    14478,
    14172,
    14169,
    13863,
    13857,
    13551,
    13548,
    13242,
    13239,
    12933,
    12927,
    12621,
    12315,
    12312,
    12303,
    12609,
    12615,
    12921,
    12924,
    13230,
    13233,
    13539,
    13845,
    13851,
    14157,
    14160,
    14466,
    14469,
    14775,
    14781,
    15087,
    15078,
    15075,
    14769,
    14766,
    14457,
    14454,
    14148,
    14145,
    13839,
    13833,
    13527,
    13221,
    13218,
    12912,
    12909,
    12603,
    12600,
    12594,
    12900,
    13206,
    13209,
    13518,
    13521,
    13827,
    13830,
    14136,
    14139,
    14445,
    14754,
    14757,
    15063,
    15066,
    15372,
    10569,
    10563,
    10560,
    10557,
    10551,
    10851,
    10848,
    10842,
    10839,
    10836,
    10833,
    10827,
    11127,
    11124,
    11121,
    11115,
    11112,
    11109,
    11106,
    11100,
    11400,
    11397,
    11394,
    11388,
    11385,
    11382,
    11079,
    11082,
    11085,
    11091,
    11094,
    11097,
    10797,
    10803,
    10806,
    10809,
    10812,
    10818,
    10821,
    10521,
    10527,
    10530,
    10533,
    10536,
    10239,
    10242,
    10245,
    10248,
    10254,
    10257,
    10260,
    9963,
    9660,
    9654,
    9651,
    9648,
    9945,
    9942,
    9939,
    9936,
    9930,
    9927,
    10227,
    10224,
    10218,
    10215,
    10212,
    10206,
    10506,
    10503,
    10500,
    10497,
    10491,
    10488,
    10788,
    10782,
    10779,
    10776,
    10473,
    10173,
    10176,
    10182,
    10185,
    10188,
    10194,
    10197,
    9897,
    9900,
    9903,
    9909,
    9912,
    9915,
    9618,
    9621,
    9624,
    9627,
    9630,
    9636,
    9336,
    9342,
    9345,
    9348,
    9351,
    9054,
    8751,
    8745,
    9045,
    9042,
    9036,
    9033,
    9030,
    9330,
    9324,
    9321,
    9318,
    9315,
    9309,
    9609,
    9606,
    9600,
    9597,
    9594,
    9591,
    9888,
    9885,
    9882,
    9879,
    9873,
    9870,
    9867,
    9564,
    9567,
    9267,
    9273,
    9276,
    9279,
    9282,
    9288,
    8988,
    8991,
    8994,
    9000,
    9003,
    9006,
    9012,
    9015,
    8715,
    8718,
    8721,
    8727,
    8730,
    8733,
    8436,
    8439,
    8442,
    8448,
    3900,
    4200,
    4500,
    4800,
    5100,
    5097,
    5397,
    5697,
    5997,
    5994,
    6294,
    6594,
    6894,
    6891,
    7191,
    7491,
    7788,
    7785,
    8085,
    8079,
    8082,
    7782,
    7482,
    7182,
    7185,
    6885,
    6585,
    6285,
    6288,
    5988,
    5688,
    5691,
    5391,
    5094,
    4794,
    4494,
    4497,
    4197,
    4188,
    4488,
    4788,
    5088,
    5085,
    5385,
    5685,
    5985,
    5982,
    6282,
    6582,
    6576,
    6876,
    7176,
    7476,
    7473,
    7773,
    8073,
    8373,
    8364,
    8064,
    7764,
    7767,
    7467,
    7167,
    7170,
    6870,
    6570,
    6573,
    6273,
    5973,
    5676,
    5376,
    5379,
    5079,
    4779,
    4479,
    4482,
    4473,
    4773,
    5073,
    5373,
    5370,
    5670,
    5967,
    5964,
    6264,
    6564,
    6864,
    6861,
    7161,
    7461,
    7458,
    7758,
    8058,
    8358,
    8355,
    8352,
    8052,
    7752,
    7755,
    7455,
    7155,
    6855,
    6858,
    6558,
    6258,
    5958,
    5961,
    5661,
    5361,
    5061,
    5064,
    4764,
    4464,
    2994,
    2991,
    2988,
    3288,
    3282,
    3279,
    3276,
    3270,
    3267,
    3567,
    3564,
    3558,
    3555,
    3552,
    3849,
    3846,
    3843,
    3840,
    3834,
    3831,
    4131,
    4128,
    4122,
    4119,
    4116,
    4113,
    4410,
    3801,
    3804,
    3810,
    3813,
    3816,
    3516,
    3522,
    3525,
    3528,
    3531,
    3537,
    3237,
    3240,
    3243,
    3249,
    3252,
    3255,
    2958,
    2961,
    2964,
    2967,
    2973,
    2976,
    2979,
    2682,
    2685,
    2688,
    2691,
    2697,
    2094,
    2091,
    2088,
    2385,
    2382,
    2379,
    2376,
    2370,
    2367,
    2664,
    2661,
    2658,
    2655,
    2652,
    2646,
    2946,
    2943,
    2937,
    2934,
    2931,
    2928,
    3225,
    3222,
    3219,
    3213,
    3210,
    3207,
    3204,
    3501,
    3498,
    3495,
    3189,
    3192,
    3195,
    2895,
    2901,
    2904,
    2907,
    2913,
    2916,
    2616,
    2619,
    2625,
    2628,
    2631,
    2331,
    2337,
    2340,
    2343,
    2349,
    2352,
    2052,
    2055,
    2061,
    2064,
    2067,
    2073,
    2076,
    1776,
    1779,
    1785,
    1788,
    1791,
    1794,
    1188,
    1185,
    1482,
    1479,
    1476,
    1473,
    1467,
    1464,
    1764,
    1761,
    1755,
    1752,
    1749,
    1743,
    2043,
    2040,
    2037,
    2031,
    2028,
    2025,
    2322,
    2319,
    2316,
    2313,
    2307,
    2304,
    2604,
    2598,
    2595,
    2592,
    2589,
    2586,
    2286,
    2289,
    2292,
    1992,
    1998,
    2001,
    2004,
    2010,
    1710,
    1713,
    1716,
    1722,
    1725,
    1728,
    1431,
    1434,
    1437,
    1440,
    1446,
    1449,
    1149,
    1155,
    1158,
    1161,
    1164,
    1170,
    870,
    873,
    876,
    882,
    20268,
    20571,
    20874,
    21486,
    21183,
    20574,
    20274,
    19971,
    19368,
    19671,
    20277,
    20580,
    20883,
    21489,
    21792,
    21795,
    21492,
    20886,
    20583,
    20280,
    19977,
    19371,
    19374,
    19677,
    20283,
    20586,
    20889,
    21495,
    21798,
    21801,
    21498,
    20892,
    20589,
    20286,
    19680,
    19377,
    19683,
    20289,
    20592,
    20895,
    21501,
    20898,
    20595,
    20292
};
