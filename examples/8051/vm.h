#ifndef __VM_H__
#define __VM_H__

extern uint32_t VMstep(uint32_t VMID);
extern void VMpor(void);
extern void SetDbgReg(uint32_t n);
extern uint32_t GetDbgReg(void);

#endif
