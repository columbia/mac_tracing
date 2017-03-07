#ifndef _PEEKMSR_HPP
#define _PEEKMSR_HPP
extern "C" {
    #include "patchkernel.h"
}
#include <IOKit/IOService.h>
#include <IOKit/IOUserClient.h>
#include <IOKit/IOLib.h>
#include <mach/mach_types.h>
#include <mach/machine.h>
#include <pexpert/pexpert.h>
#include <libkern/OSAtomic.h>
#include <string.h>

#define DEBUG 1
#define MAXUSERS	1

class PeekMsrUserClient;

enum {
    kPeekMsrUserClientRDMSR = 0,
    kPeekMsrUserClientWRMSR = 1,
    kPeekMsrUserClientHKern = 2,
};

typedef struct {
    UInt32 msr;
    UInt32 param_hi;
	UInt32 param_lo;
} msrIOdata;

class PeekMsr : public IOService
{
    OSDeclareDefaultStructors(PeekMsr)
private:
	UInt16 mClientCount;
	PeekMsrUserClient *mClientArray[MAXUSERS];
public:
    virtual bool init(OSDictionary *dictionary = 0) override;
    virtual void free(void) override;
    virtual IOService *probe(IOService *provider, SInt32 *score) override;
    virtual bool start(IOService *provider) override;
    virtual void stop(IOService *provider) override;
	virtual IOReturn newUserClient(task_t owningTask, void * securityID, UInt32 type, IOUserClient ** handler) override;
	virtual IOReturn closeUserClient(PeekMsrUserClient *client);
    virtual void a_rdmsr(uint32_t msr, uint32_t *lo, uint32_t *hi);
    virtual void a_wrmsr(uint32_t msr, uint32_t lo, uint32_t hi);
};

class PeekMsrUserClient : public IOUserClient
{
    OSDeclareDefaultStructors(PeekMsrUserClient);
private:
	PeekMsr *mDevice;
public:
	task_t fTask;
    virtual void free(void) override;
    virtual bool start(IOService *provider) override;
    virtual void stop(IOService *provider) override;
	virtual IOReturn clientClose() override;
	virtual IOReturn clientDied() override;
	virtual bool terminate(IOOptionBits options = 0) override;
	virtual bool willTerminate(IOService *provider, IOOptionBits options) override;
	virtual bool didTerminate(IOService *provider, IOOptionBits options, bool *defer) override;
    virtual IOReturn externalMethod(uint32_t selector, IOExternalMethodArguments *arguments, IOExternalMethodDispatch *dispatch, OSObject *target, void *reference) override;
	IOReturn methodRDMSR(UInt32 *dataIn, UInt32 *dataOut, IOByteCount inputSize, IOByteCount *outputSize);
	IOReturn methodWRMSR(UInt32 *dataIn, UInt32 *dataOut, IOByteCount inputSize, IOByteCount *outputSize);

};
#endif
