//
// Created by 46473 on 2022/11/26.
//

#ifndef CN_LAB7_SOCKET_DEFS_H
#define CN_LAB7_SOCKET_DEFS_H
enum Special{
    BeginPackage = 0x23,
    EndPackage = 0x66,
    Trans = 0x88
};
enum Type{
    GetTime
};
#endif //CN_LAB7_SOCKET_DEFS_H
