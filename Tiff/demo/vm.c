#include <stdio.h>
#include <stdlib.h>
#include "config.h"
#include "vm.h"
#include <string.h>

#define IMM    (IR & ~(-1<<slot))

int tiffIOR; // global error code

// This file is usable as a template for generating a C testbench

//
static const uint32_t ROM[1172] = {
/*0000*/ 0x4C00048E, 0x4C000491, 0x96B09000, 0x36B09000, 0x56B09000, 0x05AB8318,
/*0006*/ 0x96B09000, 0x05AD8318, 0x36B09000, 0xFC909000, 0xFC9FC240, 0x9E709000,
/*000C*/ 0x68A18A08, 0x29A28608, 0x2A209000, 0x2AB09000, 0x28629A28, 0x69A09000,
/*0012*/ 0x18628628, 0x68A09000, 0x186F8A68, 0x2BE29A08, 0xAEB09000, 0x8A209000,
/*0018*/ 0x68A18A68, 0x68A18A1A, 0x69A8A218, 0x19300018, 0x6C000010, 0x6C000018,
/*001E*/ 0x6C000012, 0x4C000018, 0x6C00001C, 0x4C00001C, 0x6C000018, 0xAEB09000,
/*0024*/ 0x6C000018, 0x4C00001A, 0xAEBAC240, 0x05209000, 0x04240000, 0x07805F08,
/*002A*/ 0x05FFC240, 0x7DD09000, 0x7DD74240, 0x75D09000, 0xFC914240, 0x449E4003,
/*0030*/ 0xFD709000, 0x6A71AF08, 0x98AB8A08, 0x965AC240, 0xC3F25000, 0x09000000,
/*0036*/ 0x69A2860C, 0x2868C240, 0xFCAFD7FE, 0x68240000, 0xFDAFC918, 0x89DC2B26,
/*003C*/ 0xAC240000, 0xC130003A, 0x09000000, 0xE4000004, 0xCAE09000, 0xE4000008,
/*0042*/ 0xCAE09000, 0xE400000C, 0xEAEE4008, 0xC8B1C708, 0x0679F2B8, 0x2AB09000,
/*0048*/ 0xE400000A, 0xE4002218, 0x96B09000, 0xE4000010, 0xE4002218, 0x96B09000,
/*004E*/ 0xB9000000, 0xE4FFFFFF, 0x5C240000, 0x8A2FC314, 0xC2B09000, 0xACAFC918,
/*0054*/ 0x29A2A16A, 0x18624688, 0x88340000, 0xC2B69A6A, 0xAEBAE16A, 0x18A1A20C,
/*005A*/ 0xFA20CA68, 0x2857D000, 0xC2B68240, 0xAC6AC6AC, 0x85A09000, 0xE400000C,
/*0060*/ 0xAAE09000, 0x186AC6AC, 0x68240000, 0x8A27DD40, 0x4AB09000, 0xAEB1A16A,
/*0066*/ 0x186FC9FC, 0xC2B68240, 0x6A168240, 0xFC940000, 0xE2BAEB08, 0x24A68A40,
/*006C*/ 0x98695AA0, 0x1ABAEB08, 0xFC940000, 0xE2BAEB08, 0x24A68A40, 0x38635AA0,
/*0072*/ 0x1ABAEB08, 0x2BF25000, 0xE2BAEB08, 0x24A68A40, 0xF8A36818, 0xAEBAC240,
/*0078*/ 0xE400003F, 0x5FF278AE, 0x24A40000, 0x9E82AB08, 0x09000000, 0xE400003F,
/*007E*/ 0x5FF278AE, 0x24A40000, 0x3E82AB08, 0x09000000, 0xE4000000, 0xE400001F,
/*0084*/ 0xFD000000, 0x6A76AF40, 0x22218368, 0x20940000, 0x18625000, 0xC1300085,
/*008A*/ 0xADA6AB18, 0x18A09000, 0x8A22EB40, 0x21300090, 0xAEBAF900, 0xFC109000,
/*0090*/ 0xE400001F, 0xFCF9D000, 0x69A6AF18, 0xBC84C098, 0x07E2EB40, 0x21300097,
/*0096*/ 0xF8B40000, 0x4C00009A, 0xF8BE4000, 0x9EB40000, 0x18625000, 0xC1300092,
/*009C*/ 0xAEB2AFFE, 0x8A27DA88, 0x69B00034, 0x69B0003D, 0x19B0008C, 0x28615000,
/*00A2*/ 0x493000A4, 0xFC940000, 0x28615000, 0x493000A7, 0xFC940000, 0x09000000,
/*00A8*/ 0x05A8A27C, 0x68169B34, 0x69B0003D, 0x19B0008C, 0x28615000, 0x493000AF,
/*00AE*/ 0xFC940000, 0x28615000, 0x493000B5, 0xFC989000, 0x493000B5, 0xF9A28628,
/*00B4*/ 0x2CAFC9FC, 0x1AB09000, 0x04505A40, 0x493000BA, 0xFC969B3A, 0x19000000,
/*00BA*/ 0x68115000, 0x493000BD, 0xF8340000, 0x19B0008C, 0x1924C0C0, 0x2BF24A40,
/*00C0*/ 0x09000000, 0x885293B6, 0x6C0000C1, 0xAC240000, 0x6C0000C1, 0x2AB09000,
/*00C6*/ 0x8A27C540, 0x48B14240, 0x2AB14240, 0x293000C6, 0x8A27C540, 0x48B14240,
/*00CC*/ 0xAC509000, 0x293000CA, 0x8A26C0CA, 0x48A40000, 0xAC240000, 0x8A229BCA,
/*00D2*/ 0x48A40000, 0xAC240000, 0x8A26C0C6, 0x48A40000, 0xAC240000, 0x8A229BC6,
/*00D8*/ 0x48A40000, 0xAC240000, 0x68AF8328, 0x18B09000, 0x88B68B18, 0x4C0000C6,
/*00DE*/ 0x293000C6, 0x293000CA, 0x6C000082, 0xAC240000, 0x8A27C568, 0x6C000034,
/*00E4*/ 0x29B00034, 0x6C000082, 0x1924C0E8, 0x6C00003A, 0x09000000, 0x69B000E2,
/*00EA*/ 0x193000B6, 0x6C0000E9, 0x2AB09000, 0x10000006, 0x09000000, 0x89AE4004,
/*00F0*/ 0xC9AE6214, 0xB9AE4000, 0xAB902214, 0x96B6C039, 0x1B902214, 0x96B1AB19,
/*00F6*/ 0x7C240000, 0x052AC240, 0xE4002214, 0xBAD19000, 0xE4002214, 0x96B18A68,
/*00FC*/ 0xD6B2AB18, 0x18A09000, 0x09000000, 0x09000000, 0x04400004, 0x09000000,
/*0102*/ 0xE400000A, 0x6C0000E0, 0x6C000100, 0x0D000000, 0x6C0000FE, 0x05B00100,
/*0108*/ 0x6C0000CA, 0x49300106, 0xAC240000, 0x6C0000FE, 0x04400003, 0x4930010B,
/*010E*/ 0x10000002, 0xAC240000, 0x4C00014E, 0x39300110, 0xFF0A0D02, 0x325B1B04,
/*0114*/ 0xFFFFFF4A, 0xE4000448, 0x4C000111, 0xE400044C, 0x4C000111, 0x04400000,
/*011A*/ 0x09000000, 0x04400001, 0x09000000, 0x0000042C, 0x00000454, 0x0000045C,
/*0120*/ 0x00000464, 0x0000046C, 0xE4000474, 0xE4002230, 0x96B09000, 0x9E7E6230,
/*0126*/ 0xB83B9A08, 0xE4000000, 0x4C000125, 0xE4000001, 0x4C000125, 0xE4000002,
/*012C*/ 0x4C000125, 0xE4000003, 0x4C000125, 0xE4000004, 0x4C000125, 0x89AFC9FC,
/*0132*/ 0x289286DA, 0xE4000006, 0x6C000078, 0x69B00131, 0xE400003F, 0x5C60C240,
/*0138*/ 0x6C000131, 0x079000C0, 0x5F900080, 0x7DD40000, 0x4930013E, 0xAD300138,
/*013E*/ 0x07900080, 0x5D24C14D, 0x07900020, 0x5D24C14B, 0x07900010, 0x5D24C148,
/*0144*/ 0xE4000007, 0x5DB00133, 0x6C000133, 0x4C000133, 0xE400000F, 0x5DB00133,
/*014A*/ 0x4C000133, 0xE400001F, 0x5D300133, 0x09000000, 0x07F27F14, 0xFD24C153,
/*0150*/ 0x6C000138, 0x6C000127, 0x4C00014E, 0xAEB09000, 0xE4000020, 0x4C000127,
/*0156*/ 0xFC940000, 0xE2B09000, 0x6C000154, 0x25300157, 0x09000000, 0xE4000009,
/*015C*/ 0x89B000CA, 0xE4000007, 0x5C3E4030, 0x0C240000, 0xE4002354, 0xE4002288,
/*0162*/ 0x96B09000, 0xE4002288, 0xBBF27F04, 0xE4002288, 0x96B36B08, 0xE4000000,
/*0168*/ 0xE4002218, 0xB9B0008C, 0x6B902218, 0xB9B0008C, 0x29B0015B, 0x6C000163,
/*016E*/ 0x18240000, 0x6C000167, 0x8A26C038, 0x7524C16F, 0x09000000, 0x1524C176,
/*0174*/ 0xE400002D, 0x6C000163, 0x09000000, 0xAEBE6288, 0xBB902354, 0x88B09000,
/*017A*/ 0x88B6C156, 0x4C000110, 0x68169B3D, 0x6C000160, 0x6C00016F, 0x19B00173,
/*0180*/ 0x6C000177, 0x1930017A, 0xE4000000, 0x2930017C, 0x69B00029, 0x1930017C,
/*0186*/ 0xE4000000, 0x6C00017C, 0x4C000154, 0xE4000000, 0x4C000186, 0xE4002218,
/*018C*/ 0xBB90000A, 0x7D24C18F, 0x4C000189, 0x6C000029, 0x4C000186, 0xB930018B,
/*0192*/ 0x6C000160, 0xFC940000, 0x69B00167, 0x18940000, 0xC1300194, 0xADB0016F,
/*0198*/ 0x4C000177, 0xE4002218, 0xB9A6C04B, 0xE4000000, 0x29B00192, 0x1B902218,
/*019E*/ 0x96B6C110, 0x4C000154, 0x10000005, 0x09000000, 0x69B001A0, 0xAF902288,
/*01A4*/ 0x05A96B18, 0x39A39AD8, 0x6C0001A0, 0xAC66C1A0, 0xAC618340, 0x6C0001A0,
/*01AA*/ 0xAC240000, 0xE4002288, 0x06E68A68, 0x69B001A0, 0xAFE96B18, 0x39A39AD8,
/*01B0*/ 0x6C0001A0, 0xAC66C1A0, 0xAC618340, 0x6C0001A0, 0xAC6E6288, 0x96B09000,
/*01B6*/ 0xE4000005, 0x6C0001A0, 0xAF9001FF, 0x4C0001A0, 0x6C0001B6, 0xE4000001,
/*01BC*/ 0x5DD40000, 0x493001BA, 0x09000000, 0xE400009F, 0x6C0001A0, 0x05F6C1A0,
/*01C2*/ 0x05F6C1A0, 0x05F4C1A0, 0xE4000106, 0x6C0001A0, 0xAF900020, 0xE4000100,
/*01C8*/ 0x6C0001AB, 0xE4000104, 0x6C0001A0, 0xAD3001BA, 0xE4000106, 0x6C0001A0,
/*01CE*/ 0xACAE4002, 0xE4000000, 0x6C0001AB, 0xFC9FDA38, 0xF9D40000, 0x493001DA,
/*01D4*/ 0xE4000100, 0x0DB001A0, 0xAF900104, 0x6C0001A0, 0xADB001BA, 0x1AB09000,
/*01DA*/ 0x6C0001A0, 0xAC64C1D1, 0x09000000, 0x052AEBAE, 0x8BFE40FF, 0x5C989A40,
/*01E0*/ 0x6C0000CE, 0x68A8BE40, 0x6C0001CC, 0x2BE0C618, 0x28B4C1DD, 0x09000000,
/*01E6*/ 0x2B902288, 0x96BE6288, 0x2B900004, 0x4C0001DD, 0xE40000FF, 0x6C0001A0,
/*01EC*/ 0x28D09000, 0xE400000B, 0xE4000000, 0x6C0001AB, 0x05B001A0, 0xAF902288,
/*01F2*/ 0x6C0001EA, 0x6C0001EA, 0x6C0001EA, 0x6C0001EA, 0xAF9001FF, 0x6C0001A0,
/*01F8*/ 0xAF902288, 0xB8240000, 0xE4000000, 0xE4002263, 0x36B09000, 0xE4000001,
/*01FE*/ 0xE4002263, 0x36B09000, 0xE4002263, 0xD924C204, 0xE4002220, 0x09000000,
/*0204*/ 0xE4002224, 0x09000000, 0x05AB9000, 0x6C0001E6, 0xE4000004, 0x19300005,
/*020A*/ 0xE4002220, 0x4C000206, 0xE400221C, 0x4C000206, 0xE4002224, 0x05ABA5AC,
/*0210*/ 0xE4000004, 0x19300005, 0xE4002263, 0xD924C215, 0x4C00020A, 0x4C00020E,
/*0216*/ 0x6B902288, 0x2A236BF8, 0xBB900001, 0x6C0001DD, 0xE4000001, 0x19300005,
/*021C*/ 0xE4002220, 0x4C000216, 0xE400221C, 0x4C000216, 0xE4002224, 0x05AB8DAC,
/*0222*/ 0xE4000001, 0x19300005, 0xE4002263, 0xD924C227, 0x4C00021C, 0x4C000220,
/*0228*/ 0xE4000000, 0xE4002263, 0x36B09000, 0xE4000001, 0xE4002263, 0x36B09000,
/*022E*/ 0x4C000272, 0x4C00027B, 0xE4000000, 0xE4002258, 0x96BE401A, 0xE400224A,
/*0234*/ 0x56BE4000, 0xE4002249, 0x36B09000, 0x2B90224A, 0xD9B00078, 0xE4002258,
/*023A*/ 0xB830D000, 0xE4002258, 0x96B09000, 0x6C00022E, 0xE400224A, 0xD9D8B93C,
/*0240*/ 0x5D740000, 0x49300243, 0x6C00022F, 0xE4000000, 0x6C000237, 0xE400224A,
/*0246*/ 0xD8140000, 0x4930024C, 0xE4000006, 0x2C117F5C, 0xE400224A, 0x36B09000,
/*024C*/ 0xAD30022F, 0x29B0022E, 0xE4000001, 0xE400224A, 0xD8169B78, 0x89B000CA,
/*0252*/ 0x19D6C038, 0x49300255, 0x6C00022F, 0x6C000237, 0xE400224A, 0xDB900018,
/*0258*/ 0x6C000078, 0xE4002220, 0xB83E6250, 0x96BE4000, 0xE400224A, 0x36B4C22F,
/*025E*/ 0x05AC3F24, 0x05000000, 0xE5FFFFFF, 0xFE75D000, 0x4930026A, 0x1AB07918,
/*0264*/ 0x6C00007D, 0xE4000039, 0x6C00024D, 0xE4FFFFFF, 0x5F900019, 0x4C00024D,
/*026A*/ 0x18540000, 0x49300270, 0xFC9FF939, 0x6C00024D, 0xE400003F, 0x4C00023D,
/*0270*/ 0xE4000039, 0x4C00024D, 0xE400224B, 0xD924C278, 0xE4000000, 0xE400224B,
/*0276*/ 0x36BE6254, 0xB9B0025E, 0xE4000000, 0xE4002249, 0x36B09000, 0x6C00022E,
/*027C*/ 0xE400224A, 0xD81E4015, 0x2F8AEB08, 0xAD24C282, 0xE4000010, 0x6C00023D,
/*0282*/ 0xE4002258, 0xB9B0020A, 0x4C000230, 0x6C00022E, 0x05000000, 0xE5FFFFFF,
/*0288*/ 0xFE75D000, 0x4930028B, 0x4C00025E, 0xE4002254, 0x96BE4001, 0xE400224B,
/*028E*/ 0x36B09000, 0x1C7E401B, 0x6C00024D, 0xE400225C, 0xBB900004, 0x0F6E4080,
/*0294*/ 0x5F902249, 0x36B09000, 0xE4002249, 0xD924C2A3, 0xE4002250, 0x6C00004E,
/*029A*/ 0xE4000008, 0xE4002253, 0xD9B00078, 0xFCA6C1E6, 0xE4000000, 0xE4002249,
/*02A0*/ 0x36BE4000, 0xE4002250, 0x96B09000, 0xE4000002, 0x4C00023D, 0x6C000027,
/*02A6*/ 0x493002B2, 0x6C000129, 0x6C00018B, 0x4C0002AC, 0x72687407, 0x203A776F,
/*02AC*/ 0xE4000AA8, 0x39B00110, 0xE4002238, 0x6C000032, 0x6C000110, 0x6C000129,
/*02B2*/ 0x09000000, 0x07900061, 0xE400007B, 0x6C0000DC, 0xE4000020, 0x5CB09000,
/*02B8*/ 0x69B002B3, 0xE4000030, 0x2C1E400A, 0xE4000011, 0x6C0000DC, 0x75A07909,
/*02BE*/ 0x6C0000DF, 0xE4000007, 0x5CB1A218, 0x6C0000C6, 0x5C240000, 0x0524C2D1,
/*02C4*/ 0x6816B640, 0xE4002218, 0xB9B002B8, 0x493002D0, 0x2B902218, 0xB9B00082,
/*02CA*/ 0xADA28628, 0xE4002218, 0xB9B00082, 0x6C000036, 0x1891BF24, 0xFD3002C3,
/*02D0*/ 0xAC619000, 0x09000000, 0xE4002238, 0x4C000032, 0x6C0002D2, 0xE4002240,
/*02D6*/ 0xB93000DA, 0x69000000, 0x0521AB08, 0x8B6F9F74, 0x486AC240, 0xE4000001,
/*02DC*/ 0x6C0000DA, 0x4C0002D8, 0x09000000, 0x69000000, 0x0521AB08, 0x8B6F9F40,
/*02E2*/ 0x486AC240, 0xE4000001, 0x6C0000DA, 0x4C0002E0, 0x09000000, 0x6C0002D4,
/*02E8*/ 0x89A68A18, 0x29B002DF, 0x19A28628, 0x88B68A18, 0x29D77901, 0x5E20D000,
/*02EE*/ 0xE4002240, 0x4C000005, 0x69B002D4, 0x8BE29A40, 0x6C0002D7, 0xAC62D000,
/*02F4*/ 0xE4002240, 0x6C000005, 0x193002E7, 0xE4000020, 0x4C0002F0, 0xE4000029,
/*02FA*/ 0x6C0002E7, 0x4C000110, 0x6C0002F7, 0x6C000138, 0x2AB2AB08, 0x6C00003F,
/*0300*/ 0x89F75000, 0x4AB05F08, 0xAE269B3F, 0x68AFC940, 0x68E68A38, 0x1B90224D,
/*0306*/ 0xD9D40000, 0x4930030A, 0x6C0002B3, 0x29B002B3, 0x7D24C30D, 0x1ABAEB18,
/*030C*/ 0x18105F08, 0x18940000, 0xC1300304, 0xAEBAC6AC, 0x18909000, 0xBA2E401F,
/*0312*/ 0x6C0000DE, 0xE4000012, 0xFD76C2A5, 0x89205F08, 0x05A84E40, 0xE400003F,
/*0318*/ 0x5DB002FF, 0x4930031B, 0x18240000, 0x19B0004E, 0x05D40000, 0x49300316,
/*031E*/ 0x09000000, 0xE4002248, 0xD9000000, 0xFC9FD000, 0xC2BAC240, 0x6BE9E740,
/*0324*/ 0xE4002264, 0x0EE6C311, 0x6C000027, 0x49300329, 0x1817CA08, 0x1814C321,
/*032A*/ 0x09000000, 0xE4000000, 0xE400224D, 0x36B09000, 0xE4000001, 0xE400224D,
/*0330*/ 0x36B09000, 0x8B6E402D, 0x7DD05A40, 0x49300336, 0xE4000001, 0x6C0000DA,
/*0336*/ 0x18240000, 0xE4000000, 0xE4000000, 0x6C000018, 0x6C0002C3, 0x75DE400C,
/*033C*/ 0xFD76C2A5, 0xAEB09000, 0x6B900006, 0x6C000078, 0x18E079C0, 0x5F900080,
/*0342*/ 0x7DD7790C, 0xFD76C2A5, 0xE400003F, 0x5CA6831A, 0x8B6E40C0, 0x6C0000CA,
/*0348*/ 0x8B900001, 0x7DD5D000, 0x4930034C, 0xAF609000, 0x8B6E40E0, 0x6C0000CA,
/*034E*/ 0x8B900002, 0x7DD5D000, 0x49300354, 0xACEE401F, 0x5CA6C33E, 0xAC240000,
/*0354*/ 0x8B6E40F0, 0x6C0000CA, 0x8B900003, 0x7DD5D000, 0x4930035D, 0xACEE401F,
/*035A*/ 0x5CA6C33E, 0x6C00033E, 0xAC240000, 0xE400000C, 0xFD3002A5, 0x6C000331,
/*0360*/ 0x6A28B92E, 0x6C0002DF, 0x2AB40000, 0x49300366, 0xE4000027, 0xFDB002A5,
/*0366*/ 0x07900002, 0x6C0000DF, 0x49300383, 0x8A2883FC, 0x27FD8AD8, 0x89F74A40,
/*036C*/ 0xE4000027, 0x7DD5D000, 0x49300376, 0xE4000001, 0x6C0000DA, 0xFC9FD000,
/*0372*/ 0x6C000346, 0x1924C375, 0xFC940000, 0x09000000, 0x8B6E4024, 0x7DD40000,
/*0378*/ 0x49300383, 0xE4002218, 0xB9A6C04B, 0xE4000001, 0x6C0000DA, 0x6C000337,
/*037E*/ 0x1B902218, 0x96B19000, 0x49300382, 0xFC940000, 0x09000000, 0x6C000337,
/*0384*/ 0x1924C386, 0xFC940000, 0x09000000, 0x6C0002F7, 0x0524C39E, 0x6C00031F,
/*038A*/ 0x8924C390, 0x6C00035F, 0xE4002228, 0xB924C38F, 0x6C000285, 0x4C00039A,
/*0390*/ 0x2AB05000, 0xE400225C, 0x96BE6228, 0xB9D77904, 0x5E12DB4E, 0x05000000,
/*0396*/ 0xE4800000, 0x5DD77914, 0xFD76C2A5, 0x6C000039, 0x6C000043, 0x17900003,
/*039C*/ 0xFD76C2A5, 0x4C000387, 0xAEB09000, 0x6C000043, 0x6C000027, 0x493003A7,
/*03A2*/ 0xFC940000, 0x07F25B46, 0x6C00018B, 0x2704C3A3, 0xAD000000, 0x4C0003AA,
/*03A8*/ 0x70733C04, 0xFFFFFF20, 0xE4000EA0, 0x39B00110, 0x4C000129, 0xB9000000,
/*03AE*/ 0x05A84E40, 0xE400001F, 0x5DB00110, 0x6C000154, 0x19B0004E, 0x05D40000,
/*03B4*/ 0x493003AE, 0xAC240000, 0xE4002248, 0xD9000000, 0xFC9FF0AE, 0x0679D000,
/*03BA*/ 0xE4002264, 0x0EE6C3AD, 0x4C0003B8, 0x09000000, 0x2B900003, 0xFD729000,
/*03C0*/ 0x0524C3DF, 0x6C000129, 0x8B900003, 0x6C000199, 0x8A2E4004, 0x05A6C0CE,
/*03C6*/ 0x1A22F909, 0x6C0000E0, 0x69000000, 0x0524C3CE, 0x6A6E4007, 0x6C000199,
/*03CC*/ 0x1BF27F40, 0x4C0003C9, 0xAEB19000, 0x6C000156, 0x8A2E4010, 0x6C0000CE,
/*03D2*/ 0x0524C3DA, 0x68E07920, 0xE40000C0, 0x6C0000DC, 0x4ABE402E, 0x6C000127,
/*03D8*/ 0x1BF27F40, 0x4C0003D2, 0xAEBE4010, 0x6C0000DA, 0xE4000000, 0x6C0000D1,
/*03DE*/ 0x4C0003C0, 0xAEB4C129, 0x05000000, 0x0524C3E5, 0x68A18A18, 0x29A6BF24,
/*03E4*/ 0xFD3003E1, 0xAC629A6A, 0x18629A04, 0x0524C3EB, 0x18629A28, 0x68A1BF24,
/*03EA*/ 0xFD3003E7, 0xAC240000, 0x4C0003F0, 0x6C65480B, 0x57206F6C, 0x646C726F,
/*03F0*/ 0xE4000FB4, 0x39B00110, 0x6C000129, 0xE400000A, 0xE4000000, 0x2BF25A68,
/*03F6*/ 0xF9B0018B, 0x6C000055, 0x4C0003F6, 0x09000000, 0xE400225C, 0xBBF87F40,
/*03FC*/ 0x4C00004E, 0x4C000407, 0x4C00043C, 0x4C00044F, 0x4C000409, 0x4C000453,
/*0402*/ 0x4C000404, 0x4C000451, 0xE400225C, 0xBB900010, 0x2EE09000, 0x6C0003FA,
/*0408*/ 0x4C000039, 0xE400225C, 0xBB900014, 0x2DB00032, 0x4C000039, 0xE400000D,
/*040E*/ 0xFD3002A5, 0x6C000402, 0xE8240000, 0x04240000, 0x0C240000, 0x2C240000,
/*0414*/ 0x8C240000, 0xF8240000, 0x5C240000, 0x88240000, 0x18240000, 0x7C240000,
/*041A*/ 0x9C240000, 0xBC240000, 0x34240000, 0x38240000, 0x38240000, 0xD8240000,
/*0420*/ 0x54240000, 0x58240000, 0x78240000, 0x94240000, 0x98240000, 0xB8240000,
/*0426*/ 0x3C240000, 0x80240000, 0xA0240000, 0x1C240000, 0xC8240000, 0xFC240000,
/*042C*/ 0xB4240000, 0xA8240000, 0xDC240000, 0xD4240000, 0xE8240000, 0xF4240000,
/*0432*/ 0xAC240000, 0x24240000, 0x24240000, 0x44240000, 0x84240000, 0x68240000,
/*0438*/ 0x28240000, 0x74240000, 0x14240000, 0x09000000, 0x6C0003FA, 0x9B90001A,
/*043E*/ 0x04540000, 0x49300444, 0xAF900003, 0x5DA9B91A, 0x19000000, 0x4C000448,
/*0444*/ 0x8A26C07D, 0xE400003F, 0x5DAE4006, 0x2C640000, 0x07900002, 0x7DD40000,
/*044A*/ 0x4930044C, 0xAEBAEB08, 0x6C00023D, 0x4C00043E, 0x09000000, 0x6C0003FA,
/*0450*/ 0x4C00028F, 0x6C000402, 0x4C000285, 0xE400225C, 0xBB900014, 0x2E66C285,
/*0456*/ 0xB930028F, 0x00000004, 0x00002204, 0x00002200, 0x000021F0, 0x00002100,
/*045C*/ 0x00000000, 0x00000005, 0x00002218, 0x0000000A, 0x00004498, 0x0000115C,
/*0462*/ 0x00002354, 0x00000000, 0x00000004, 0x0000222C, 0x00002284, 0x00000474,
/*0468*/ 0x00000001, 0x00000000, 0x00000002, 0x0000223C, 0x0000228C, 0x00000000,
/*046E*/ 0x00000002, 0x00002248, 0x001A0001, 0x00000000, 0x00000002, 0x00002254,
/*0474*/ 0x00000014, 0x00000000, 0x00000004, 0x0000225C, 0x00002A64, 0x00090003,
/*047A*/ 0x00002284, 0x00000000, 0x00000002, 0x00002284, 0x00004480, 0x00000000,
/*0480*/ 0x00000000, 0xE400115C, 0x9A28899C, 0x9C329A28, 0x18140000, 0x49300488,
/*0486*/ 0x6A619B69, 0x4C000482, 0x1B902288, 0x96BE6200, 0xF7902100, 0xD79021EC,
/*048C*/ 0xB7902288, 0xB9A09000, 0x6C000481, 0x6C0003EC, 0x4C0000ED, 0x6C000481,
/*0492*/ 0x6C0003EC, 0x4C0000ED};
//
uint32_t FetchROM(uint32_t addr) {
//
  if (addr < 1172) {
//
    return ROM[addr];
//
  }
//
  return -1;
//
}

/// Virtual Machine for 32-bit MachineForth.

/// The VM registers are defined here but generally not accessible outside this
/// module except through VMstep. The VM could be at the end of a cable, so we
/// don't want direct access to its innards.

/// These functions are always exported: VMpor, VMstep, SetDbgReg, GetDbgReg.
/// If TRACEABLE, you get more exported: UnTrace, VMreg[],
/// while importing the Trace function. This offers a way to break the
/// "no direct access" rule in a PC environment, for testing and VM debugging.

/// Flash writing is handled by streaming data to AXI space, through VMstep and
/// friends. ROM writing uses a WriteROM function exported to Tiff but not used
/// in a target system.

/// The optional Trace function tracks state changes using these parameters:
/// Type of state change: 0 = unmarked, 1 = new opcode, 2 or 3 = new group;
/// Register ID: Complement of register number if register, memory if other;
/// Old value: 32-bit.
/// New value: 32-bit.

#ifdef TRACEABLE
    #define T  VMreg[0]
    #define N  VMreg[1]
    #define RP VMreg[2]
    #define SP VMreg[3]
    #define UP VMreg[4]
    #define PC VMreg[5]
    #define DebugReg VMreg[6]
    #define CARRY    VMreg[7]
    #define RidT   (-1)
    #define RidN   (-2)
    #define RidRP  (-3)
    #define RidSP  (-4)
    #define RidUP  (-5)
    #define RidPC  (-6)
    #define RidDbg (-7)
    #define RidCY  (-8)
    #define VMregs 10

    uint32_t VMreg[VMregs];     // registers with undo capability
    uint32_t OpCounter[64];     // opcode counter
    uint32_t ProfileCounts[ROMsize];
    uint32_t cyclecount;

    static int New; // New trace type, used to mark new sections of trace
    static uint32_t RAM[RAMsize];
//
#ifdef __NEVER_INCLUDE__
    static uint32_t ROM[ROMsize];
//
#endif
    uint32_t AXI[SPIflashSize+AXIRAMsize];

    static void SDUP(void)  {
        Trace(New,RidSP,SP,SP-1); New=0;
                     --SP;
        Trace(0,SP & (RAMsize-1),RAM[SP & (RAMsize-1)],  N);
                                 RAM[SP & (RAMsize-1)] = N;
        Trace(0, RidN, N,  T);
                       N = T; }
    static void SDROP(void) {
        Trace(New,RidT,T,  N); New=0;
                       T = N;
        Trace(0, RidN, N,  RAM[SP & (RAMsize-1)]);
                       N = RAM[SP & (RAMsize-1)];
        Trace(0,RidSP,SP,SP+1);
                   SP++; }
    static void SNIP(void)  {
        Trace(New,RidN,N,  RAM[SP & (RAMsize-1)]);  New=0;
                       N = RAM[SP & (RAMsize-1)];
        Trace(0,RidSP, SP,SP+1);
                       SP++; }
    static void RDUP(uint32_t x)  {
        Trace(New,RidRP,RP,RP-1); New=0;
                       --RP;
        Trace(0,RP & (RAMsize-1),RAM[RP & (RAMsize-1)],  x);
                                 RAM[RP & (RAMsize-1)] = x; }
    static uint32_t RDROP(void) {
        uint32_t r = RAM[RP & (RAMsize-1)];
        Trace(New,RidRP, RP,RP+1);  New=0;
                         RP++;  return r; }

#else
    static uint32_t T;	    static uint32_t RP = 64;
    static uint32_t N;	    static uint32_t SP = 32;
    static uint32_t PC;     static uint32_t UP = 64;
    static uint32_t CARRY;  static uint32_t DebugReg;

    static uint32_t RAM[RAMsize];
//
#ifdef __NEVER_INCLUDE__
    static uint32_t ROM[ROMsize];
//
#endif
    uint32_t AXI[SPIflashSize+AXIRAMsize];

    static void SDUP(void)  { RAM[--SP & (RAMsize-1)] = N;  N = T; }
    static void SDROP(void) { T = N;  N = RAM[SP++ & (RAMsize-1)]; }
    static void SNIP(void)  { N = RAM[SP++ & (RAMsize-1)]; }
    static void RDUP(uint32_t x)  { RAM[--RP & (RAMsize-1)] = x; }
    static uint32_t RDROP(void) { return RAM[RP++ & (RAMsize-1)]; }

#endif // TRACEABLE

//
#ifdef __NEVER_INCLUDE__
/// Tiff's ROM write functions for populating internal ROM.
/// A copy may be stored to SPI flash for targets that boot from SPI.
/// An MCU-based system will have ROM in actual ROM.
/// The target system does not use WriteROM, only the host.

int WriteROM(uint32_t data, uint32_t address) { // EXPORTED
    uint32_t addr = address / 4;
    if (address & 3) return -23;        // alignment problem
    if (addr < ROMsize)  {
        ROM[addr] = data;
        return 0;
    }
    if (addr < SPIflashSize) {          // only flash region is writable
        AXI[addr] = data;
        return 0;
    }
    return -9;                          // out of range
}
//
#endif

/// The VM's RAM and ROM are internal to this module.
/// They are both little endian regardless of the target machine.
/// RAM and ROM are accessed only through execution of an instruction
/// group. Writing to ROM is a special case, depending on a USER function
/// with the restriction that you can't turn '0's into '1's.


// Send a stream of RAM words to the AXI bus.
// The only thing on the AXI bus here is SPI flash.
// An external function could be added in the future for other stuff.
// dest is a cell address, length is 0 to 255 meaning 1 to 256 words.
// *** Modify to only support the AXIRAMsize region after SPI flash.
static void SendAXI(int src, unsigned int dest, uint8_t length) {
    uint32_t old, data;     int i;
    src -= ROMsize;
    if (src < 0) goto bogus;            // below RAM address
    if (src >= (RAMsize-length)) goto bogus;
    if (dest >= (SPIflashSize-length)) goto bogus;
    for (i=0; i<=length; i++) {
        old = AXI[dest];		        // existing flash data
        data = RAM[src++];
        if (~(old|data)) {
            tiffIOR = -60;              // not erased
            return;
        }
        AXI[dest++] = old & data;
    } return;
bogus: tiffIOR = -9;                    // out of range
}

// Receive a stream of RAM words from the AXI bus.
// The only thing on the AXI bus here is SPI flash.
// An external function could be added in the future for other stuff.
// src is a cell address, length is 0 to 255 meaning 1 to 256 words.
static void ReceiveAXI(unsigned int src, int dest, uint8_t length) {
    dest -= ROMsize;
    if (dest < 0) goto bogus;            // below RAM address
    if (dest >= (RAMsize-length)) goto bogus;  // won't fit
    if (src >= (SPIflashSize+AXIRAMsize-length)) goto bogus;
    memmove(&AXI[dest], &RAM[src], length+1);  // can read all of AXI space
    return;
bogus: tiffIOR = -9;                    // out of range
}

// Generic fetch from ROM or RAM: ROM is at the bottom, RAM is in middle, AXI is at top
static int32_t FetchX (int32_t addr, int shift, int mask) {
    if (addr < ROMsize) {
        return (ROM[addr] >> shift) & mask;
    } else {
        if (addr < (ROMsize + RAMsize)) {
            return (RAM[addr-ROMsize] >> shift) & mask;
        } else if (addr < (SPIflashSize+AXIRAMsize)) {
            return (AXI[addr] >> shift) & mask;
        } else return 0;
    }
}

// Generic store to RAM: ROM is at the bottom, RAM wraps.
static void StoreX (uint32_t addr, uint32_t data, int shift, int mask) {
    uint32_t temp;
    addr = addr & (RAMsize-1);
    temp = RAM[addr] & (~(mask << shift));
#ifdef TRACEABLE
    temp = ((data & mask) << shift) | temp;
    Trace(New, addr, RAM[addr], temp);  New=0;
    RAM[addr] = temp;
#else
    RAM[addr] = ((data & mask) << shift) | temp;
#endif // TRACEABLE
}

#ifdef TRACEABLE
    // Untrace undoes a state change by restoring old data
    void UnTrace(int32_t ID, uint32_t old) {  // EXPORTED
        int idx = ~ID;
        if (ID<0) {
            if (idx < VMregs) {
                VMreg[idx] = old;
            }
        } else {
            StoreX(ID, old, 0, 0xFFFFFFFF);
        }
    }
#endif // TRACEABLE

////////////////////////////////////////////////////////////////////////////////
/// Access to the VM is through four functions:
///    VMstep       // Execute an instruction group
///    VMpor        // Power-on reset
///    SetDbgReg    // write to the debug mailbox
///    GetDbgReg    // read from the debug mailbox
/// IR is the instruction group.
/// Paused is 0 when PC post-increments, other when not.

void VMpor(void) {  // EXPORTED
#ifdef TRACEABLE
    memset(OpCounter,0,64*sizeof(uint32_t)); // clear opcode profile counters
    memset(ProfileCounts, 0, ROMsize*sizeof(uint32_t));  // clear profile counts
    cyclecount = 0;                     // cycles since POR
#endif // TRACEABLE
    PC = 0;  RP = 64;  SP = 32;  UP = 64;
    T=0;  N=0;  DebugReg = 0;
    memset(RAM, 0, RAMsize*sizeof(uint32_t));  // clear RAM
}

uint32_t VMstep(uint32_t IR, int Paused) {  // EXPORTED
	uint32_t M;  int slot;
	uint64_t DX;
	unsigned int opcode;
// The PC is incremented at the same time the IR is loaded. Slot0 is next clock.
// The instruction group returned from memory will be latched in after the final
// slot executes. In the VM, that is simulated by a return from this function.
// Some slots may alter the PC. The last slot may alter the PC, so if it modifies
// the PC extra cycles are taken after the instruction group to resolve the
// instruction flow. If the PC has been steady long enough for the instruction
// to show up, it's latched into IR. Otherwise, there will be some delay while
// memory returns the instruction.

    if (!Paused) {
#ifdef TRACEABLE
        if (PC < ROMsize) {
            ProfileCounts[PC]++;
        }
        Trace(3, RidPC, PC, PC + 1);
#endif // TRACEABLE
        PC = PC + 1;
    }

	slot = 32;
	do { // valid slots: 26, 20, 14, 8, 2, -4
        slot -= 6;
        if (slot < 0) {
            opcode = IR & 3;                // slot = -4
        } else {
            opcode = (IR >> slot) & 0x3F;   // slot = 26, 20, 14, 8, 2
        }
#ifdef TRACEABLE
        OpCounter[opcode]++;
        New = 1;  // first state change in an opcode
        if (!Paused) cyclecount += 1;
#endif // TRACEABLE
        switch (opcode) {
			case opNOP:									break;	// nop
			case opDUP: SDUP();							break;	// dup
			case opEXIT:
                M = RDROP() >> 2;
#ifdef TRACEABLE
                Trace(New, RidPC, PC, M);  New=0;
                if (!Paused) cyclecount += 3;  // PC change flushes pipeline
#endif // TRACEABLE
                // PC is a cell address. The return stack works in bytes.
                PC = M;  goto ex;                   	        // exit
			case opADD:
			    DX = (uint64_t)N + (uint64_t)T;
#ifdef TRACEABLE
                Trace(New, RidT, T, (uint32_t)DX);  New=0;
                Trace(0, RidCY, CARRY, (uint32_t)(DX>>32));
#endif // TRACEABLE
                T = (uint32_t)DX;
                CARRY = (uint32_t)(DX>>32);
                SNIP();	                                break;	// +
			case opSKIP: goto ex;					    break;	// no:
			case opUSER: M = UserFunction (T, N, IMM);          // user
#ifdef TRACEABLE
                Trace(New, RidT, T, M);  New=0;
#endif // TRACEABLE
                T = M;  goto ex;
			case opZeroLess:
                M=0;  if ((signed)T<0) M--;
#ifdef TRACEABLE
                Trace(New, RidT, T, M);  New=0;
#endif // TRACEABLE
                T = M;                                  break;  // 0<
			case opPOP:  SDUP();  M = RDROP();
#ifdef TRACEABLE
                Trace(0, RidT, T, M);
#endif // TRACEABLE
			    T = M;      				            break;	// r>
			case opTwoDiv:
#ifdef TRACEABLE
                Trace(0, RidCY, CARRY, T&1);
                Trace(New, RidT, T, (signed)T >> 1);  New=0;
#endif // TRACEABLE
			    CARRY = T&1;  T = (signed)T >> 1;       break;	// 2/
			case opSKIPNC: if (!CARRY) goto ex;	        break;	// ifc:
			case opOnePlus:
#ifdef TRACEABLE
                Trace(New, RidT, T, T + 1);  New=0;
#endif // TRACEABLE
			    T = T + 1;                              break;	// 1+
			case opPUSH:  RDUP(T);  SDROP();            break;  // >r
			case opSUB:
			    DX = (uint64_t)N - (uint64_t)T;
#ifdef TRACEABLE
                Trace(New, RidT, T, (uint32_t)DX);  New=0;
                Trace(0, RidCY, CARRY, (uint32_t)(DX>>32));
#endif // TRACEABLE
                T = (uint32_t)DX;
                CARRY = (uint32_t)(DX>>32);
                SNIP();	                                break;	// -
			case opCstorePlus:    /* ( n a -- a' ) */
                StoreX(T>>2, N, (T&3)*8, 0xFF);
#ifdef TRACEABLE
                Trace(0, RidT, T, T+1);
#endif // TRACEABLE
                T += 1;   SNIP();                       break;  // c!+
			case opCfetchPlus:  SDUP();  /* ( a -- a' c ) */
                M = FetchX(N>>2, (N&3) * 8, 0xFF);
#ifdef TRACEABLE
                Trace(0, RidT, T, M);
                Trace(0, RidN, N, N+1);
#endif // TRACEABLE
                T = M;
                N += 1;                                 break;  // c@+
			case opUtwoDiv:
#ifdef TRACEABLE
                Trace(New, RidT, T, (unsigned) T / 2);  New=0;
                Trace(0, RidCY, CARRY, T&1);
#endif // TRACEABLE
			    T = T / 2;   CARRY = T&1;               break;	// u2/
			case opTwoPlus:
#ifdef TRACEABLE
                Trace(New, RidT, T, T + 2);  New=0;
#endif // TRACEABLE
			    T = T + 2;                              break;	// 2+
			case opOVER: M = N;  SDUP();
#ifdef TRACEABLE
                Trace(0, RidT, T, M);
#endif // TRACEABLE
                T = M;				                    break;	// over
			case opJUMP:
#ifdef TRACEABLE
                Trace(New, RidPC, PC, IMM);  New=0;
                if (!Paused) cyclecount += 3;
				// PC change flushes pipeline in HW version
#endif // TRACEABLE
                // Jumps and calls use cell addressing
			    PC = IMM;  goto ex;                             // jmp
			case opWstorePlus:    /* ( n a -- a' ) */
                StoreX(T>>2, N, (T&2)*8, 0xFFFF);
#ifdef TRACEABLE
                Trace(0, RidT, T, T+2);
#endif // TRACEABLE
                T += 2;   SNIP();                       break;  // w!+
			case opWfetchPlus:  SDUP();  /* ( a -- a' c ) */
                M = FetchX(N>>2, (N&2) * 8, 0xFFFF);
#ifdef TRACEABLE
                Trace(0, RidT, T, M);
                Trace(0, RidN, N, N+2);
#endif // TRACEABLE
                T = M;
                N += 2;                                 break;  // w@+
			case opAND:
#ifdef TRACEABLE
                Trace(New, RidT, T, T & N);  New=0;
#endif // TRACEABLE
                T = T & N;  SNIP();	                    break;	// and
            case opLitX:
				M = (T<<24) | (IMM & 0xFFFFFF);
#ifdef TRACEABLE
                Trace(New, RidT, T, M);  New=0;
#endif // TRACEABLE
                T = M;
                goto ex;                                        // litx
			case opSWAP: M = N;                                 // swap
#ifdef TRACEABLE
                Trace(New, RidN, N, T);  N = T;  New=0;
                Trace(0, RidT, T, M);    T = M;         break;
#else
                N = T;  T = M;  break;
#endif // TRACEABLE
			case opCALL:  RDUP(PC<<2);                        	// call
#ifdef TRACEABLE
                Trace(0, RidPC, PC, IMM);  PC = IMM;
                if (!Paused) cyclecount += 3;
                goto ex;
#else
                PC = IMM;  goto ex;
#endif // TRACEABLE
            case opZeroEquals:
                M=0;  if (T==0) M--;
#ifdef TRACEABLE
                Trace(New, RidT, T, M);  New=0;
#endif // TRACEABLE
                T = M;                                  break;  // 0=
			case opWfetch:  /* ( a -- w ) */
                M = FetchX(T>>2, (T&2) * 8, 0xFFFF);
#ifdef TRACEABLE
                Trace(0, RidT, T, M);
#endif // TRACEABLE
                T = M;                                  break;  // w@
			case opXOR:
#ifdef TRACEABLE
                Trace(New, RidT, T, T ^ N);  New=0;
#endif // TRACEABLE
                T = T ^ N;  SNIP();	                    break;	// xor
			case opREPT:  slot = 32;                    break;	// rept
			case opFourPlus:
#ifdef TRACEABLE
                Trace(New, RidT, T, T + 4);  New=0;
#endif // TRACEABLE
			    T = T + 4;                              break;	// 4+
            case opSKIPNZ:
				M = T;  SDROP();
                if (M == 0) break;
                goto ex;  										// ifz:
			case opADDC:  // carry into adder
			    DX = (uint64_t)N + (uint64_t)T + (uint64_t)(CARRY & 1);
#ifdef TRACEABLE
                Trace(New, RidT, T, (uint32_t)DX);  New=0;
                Trace(0, RidCY, CARRY, (uint32_t)(DX>>32));
#endif // TRACEABLE
                T = (uint32_t)DX;
                CARRY = (uint32_t)(DX>>32);
                SNIP();	                                break;	// c+
			case opStorePlus:    /* ( n a -- a' ) */
                StoreX(T>>2, N, 0, 0xFFFFFFFF);
#ifdef TRACEABLE
                Trace(0, RidT, T, T+4);
#endif // TRACEABLE
                T += 4;   SNIP();                       break;  // !+
			case opFetchPlus:  SDUP();  /* ( a -- a' c ) */
                M = FetchX(N>>2, 0, 0xFFFFFFFF);
#ifdef TRACEABLE
                Trace(0, RidT, T, M);
                Trace(0, RidN, N, N+4);
#endif // TRACEABLE
                T = M;
                N += 4;                                 break;  // @+
			case opTwoStar:
                M = T * 2;
#ifdef TRACEABLE
                Trace(0, RidT, T, M);
                Trace(0, RidCY, CARRY, T>>31);
#endif // TRACEABLE
                CARRY = T>>31;   T = M;                 break;  // 2*
			case opMiREPT:
                if (N&0x8000) slot = 32;          	            // -rept
#ifdef TRACEABLE
                Trace(New, RidN, N, N+1);  New=0; // repeat loop uses N
#endif // TRACEABLE                               // test and increment
                N++;  break;
			case opRP: M = RP;                                  // rp
                goto GetPointer;
			case opDROP: SDROP();		    	        break;	// drop
			case opSetRP:
                M = (T>>2) & (RAMsize-1);
#ifdef TRACEABLE
                Trace(New, RidRP, RP, M);  New=0;
#endif // TRACEABLE
			    RP = M;  SDROP();                       break;	// rp!
			case opFetch:  /* ( a -- n ) */
                M = FetchX(T>>2, 0, 0xFFFFFFFF);
#ifdef TRACEABLE
                Trace(0, RidT, T, M);
#endif // TRACEABLE
                T = M;                                  break;  // @
            case opTwoStarC:
                M = (T*2) | (CARRY&1);
#ifdef TRACEABLE
                Trace(0, RidT, T, M);
                Trace(0, RidCY, CARRY, T>>31);

#endif // TRACEABLE
                CARRY = T>>31;   T = M;                 break;  // 2*c
			case opSKIPGE: if ((signed)T < 0) break;            // -if:
                goto ex;
			case opSP: M = SP;                                  // sp
GetPointer:     M = T + (M + ROMsize)*4;
#ifdef TRACEABLE
                Trace(0, RidT, T, M);
#endif // TRACEABLE
			    T = M;                                  break;
			case opFetchAS:
                ReceiveAXI(N/4, T/4, IMM);  goto ex;	        // @as
			case opSetSP:
                M = (T>>2) & (RAMsize-1);
#ifdef TRACEABLE
                Trace(New, RidSP, SP, M);  New=0;
#endif // TRACEABLE
                // SP! does not post-drop
			    SP = M;         	                    break;	// sp!
			case opCfetch:  /* ( a -- w ) */
                M = FetchX(T>>2, (T&3) * 8, 0xFF);
#ifdef TRACEABLE
                Trace(0, RidT, T, M);
#endif // TRACEABLE
                T = M;                                  break;  // c@
			case opPORT: M = T;
#ifdef TRACEABLE
                Trace(0, RidT, T, DebugReg);
                Trace(0, RidDbg, DebugReg, M);
#endif // TRACEABLE
                T=DebugReg;
                DebugReg=M;
                break;	// port
			case opSKIPLT: if ((signed)T >= 0) break;           // +if:
                goto ex;
			case opLIT: SDUP();
#ifdef TRACEABLE
                Trace(0, RidT, T, IMM);
#endif // TRACEABLE
                T = IMM;  goto ex;                              // lit
			case opUP: M = UP;  	                            // up
                goto GetPointer;
			case opStoreAS:  // ( src dest -- src dest ) imm length
                SendAXI(N/4, T/4, IMM);  goto ex;               // !as
			case opSetUP:
                M = (T>>2) & (RAMsize-1);
#ifdef TRACEABLE
                Trace(New, RidUP, UP, M);  New=0;
#endif // TRACEABLE
			    UP = M;  SDROP();	                    break;	// up!
			case opRfetch: SDUP();
                M = RAM[RP & (RAMsize-1)];
#ifdef TRACEABLE
                Trace(New, RidT, T, M);  New=0;
#endif // TRACEABLE
                T = M;					                break;	// r@
			case opCOM:
#ifdef TRACEABLE
                Trace(New, RidT, T, ~T);  New=0;
#endif // TRACEABLE
			    T = ~T;                                 break;	// com
			default:                           		    break;	//
		}
	} while (slot>=0);
ex: return PC;
}

// write to the debug mailbox
void SetDbgReg(uint32_t n) {  // EXPORTED
    DebugReg = n;
}

// read from the debug mailbox
uint32_t GetDbgReg(void) {  // EXPORTED
    return DebugReg;
}
