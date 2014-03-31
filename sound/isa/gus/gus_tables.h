/*
 *  Copyright (c) by Jaroslav Kysela <perex@perex.cz>
 *
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */

#define SNDRV_GF1_SCALE_TABLE_SIZE	128
#define SNDRV_GF1_ATTEN_TABLE_SIZE	128

#ifdef __GUS_TABLES_ALLOC__

#if 0

unsigned int snd_gf1_scale_table[SNDRV_GF1_SCALE_TABLE_SIZE] =
{
      8372,      8870,      9397,      9956,     10548,     11175,
     11840,     12544,     13290,     14080,     14917,     15804,
     16744,     17740,     18795,     19912,     21096,     22351,
     23680,     25088,     26580,     28160,     29834,     31609,
     33488,     35479,     37589,     39824,     42192,     44701,
     47359,     50175,     53159,     56320,     59669,     63217,
     66976,     70959,     75178,     79649,     84385,     89402,
     94719,    100351,    106318,    112640,    119338,    126434,
    133952,    141918,    150356,    159297,    168769,    178805,
    189437,    200702,    212636,    225280,    238676,    252868,
    267905,    283835,    300713,    318594,    337539,    357610,
    378874,    401403,    425272,    450560,    477352,    505737,
    535809,    567670,    601425,    637188,    675077,    715219,
    757749,    802807,    850544,    901120,    954703,   1011473,
   1071618,   1135340,   1202851,   1274376,   1350154,   1430439,
   1515497,   1605613,   1701088,   1802240,   1909407,   2022946,
   2143237,   2270680,   2405702,   2548752,   2700309,   2860878,
   3030994,   3211227,   3402176,   3604480,   3818814,   4045892,
   4286473,   4541360,   4811404,   5097505,   5400618,   5721755,
   6061989,   6422453,   6804352,   7208960,   7637627,   8091784,
   8572947,   9082720,   9622807,  10195009,  10801236,  11443511,
  12123977,  12844906
};

#endif  

unsigned short snd_gf1_atten_table[SNDRV_GF1_ATTEN_TABLE_SIZE] = {
  4095 ,1789 ,1533 ,1383 ,1277 ,
  1195 ,1127 ,1070 ,1021 ,978  ,
  939  ,903  ,871  ,842  ,814  ,
  789  ,765  ,743  ,722  ,702  ,
  683  ,665  ,647  ,631  ,615  ,
  600  ,586  ,572  ,558  ,545  ,
  533  ,521  ,509  ,498  ,487  ,
  476  ,466  ,455  ,446  ,436  ,
  427  ,418  ,409  ,400  ,391  ,
  383  ,375  ,367  ,359  ,352  ,
  344  ,337  ,330  ,323  ,316  ,
  309  ,302  ,296  ,289  ,283  ,
  277  ,271  ,265  ,259  ,253  ,
  247  ,242  ,236  ,231  ,225  ,
  220  ,215  ,210  ,205  ,199  ,
  195  ,190  ,185  ,180  ,175  ,
  171  ,166  ,162  ,157  ,153  ,
  148  ,144  ,140  ,135  ,131  ,
  127  ,123  ,119  ,115  ,111  ,
  107  ,103  ,100  ,96   ,92   ,
  88   ,85   ,81   ,77   ,74   ,
  70   ,67   ,63   ,60   ,56   ,
  53   ,50   ,46   ,43   ,40   ,
  37   ,33   ,30   ,27   ,24   ,
  21   ,18   ,15   ,12   ,9    ,
  6    ,3    ,0    ,
};

#else

extern unsigned int snd_gf1_scale_table[SNDRV_GF1_SCALE_TABLE_SIZE];
extern unsigned short snd_gf1_atten_table[SNDRV_GF1_ATTEN_TABLE_SIZE];

#endif
