#ifndef OPCODES_H
#define OPCODES_H

/* Plan §5.4 / LadderCompiler.js */
#define OPC_LD 0x01
#define OPC_LD_NOT 0x02
#define OPC_AND 0x03
#define OPC_AND_NOT 0x04
#define OPC_OR 0x05
#define OPC_OR_NOT 0x06
#define OPC_OUT 0x07
#define OPC_OUT_NOT 0x08
#define OPC_SET 0x09
#define OPC_RST 0x0a
#define OPC_TON 0x0b
#define OPC_TOF 0x0c
#define OPC_CTU 0x0d
#define OPC_MOV 0x0e
#define OPC_CMP_EQ 0x0f
#define OPC_CMP_GT 0x10
#define OPC_END 0xff

/* Operand1 memory class for LD/LD_NOT/AND/OR variants */
#define MEM_DISCRETE_INPUT 0u
#define MEM_COIL_READ 1u

#define PLCBIN_MAGIC 0x504c4301u

#endif
