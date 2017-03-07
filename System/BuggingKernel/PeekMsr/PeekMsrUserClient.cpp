#include "PeekMsr.hpp"
#ifdef super
#undef super
#endif
#define super IOUserClient

#define	MSR_IA32_LSTAR				0xC0000082
#define MSR_IA32_SYSENTER_EIP		0x176

OSDefineMetaClassAndStructors(PeekMsrUserClient, IOUserClient);

void PeekMsrUserClient::free(void)
{
    IOLog("PeekMsrUserClient[0x%08x%08x]::free\n",
		(uint32_t)((uint64_t)this >> 32), (uint32_t)((uint64_t)this));
	mDevice->release();
	super::free();
}

bool PeekMsrUserClient::start(IOService *provider)
{
    IOLog("PeekMsrUserClient[0x%08x%08x]::start ...\n",
		(uint32_t)((uint64_t)this >> 32), (uint32_t)((uint64_t)this));
    
	if (!super::start(provider))
		return false;
    
	mDevice = OSDynamicCast(PeekMsr, provider);
	mDevice->retain();
    IOLog("PeekMsrUserClient[0x%08x%08x]::start success\n",
		(uint32_t)((uint64_t)this >> 32), (uint32_t)((uint64_t)this));
    
	return true;
}

/* stop will be called during the termination process,
 * and should free all resources
 * associated with this client
 */
void PeekMsrUserClient::stop(IOService *provider)
{
	super::stop(provider);
}

/* called when the user process
 * calls IOServiceClose
 */
IOReturn PeekMsrUserClient::clientClose()
{
    IOLog("PeekMsrUserClient[0x%08x%08x]::clientClose\n",
		(uint32_t)((uint64_t)this >> 32), (uint32_t)((uint64_t)this));
	IOReturn ioReturn = kIOReturnError;;
    if (mDevice != NULL)
        ioReturn = mDevice->closeUserClient(this);

	if (!isInactive())
		terminate();
    
	return ioReturn;
}

/* called when the user process terminates unexpectedly */
IOReturn PeekMsrUserClient::clientDied()
{
    IOLog("PeekMsrUserClient[0x%08x%08x]::clientDied\n",
		(uint32_t)((uint64_t)this >> 32), (uint32_t)((uint64_t)this));
	return clientClose();
}

bool PeekMsrUserClient::terminate(IOOptionBits options)
{
    IOLog("PeekMsrUserClient[0x%08x%08x]::terminate\n",
		(uint32_t)((uint64_t)this >> 32), (uint32_t)((uint64_t)this));
	return super::terminate(options);
}

bool PeekMsrUserClient::willTerminate(IOService *provider, IOOptionBits options)
{
    IOLog("PeekMsrUserClient[0x%08x%08x]::willTerminate\n",
		(uint32_t)((uint64_t)this >> 32), (uint32_t)((uint64_t)this));
	return super::willTerminate(provider, options);
}

bool PeekMsrUserClient::didTerminate(IOService *provider, IOOptionBits options, bool *defer)
{
    IOLog("PeekMsrUserClient[0x%08x%08x]::didTerminate\n",
		(uint32_t)((uint64_t)this >> 32), (uint32_t)((uint64_t)this));
	// if defer is true, stop will not be called on the user client
	*defer = false;
	return super::didTerminate(provider, options, defer);
}

// defining and selecting our external user client methods
IOReturn PeekMsrUserClient::externalMethod(uint32_t selector, IOExternalMethodArguments* arguments,
			IOExternalMethodDispatch* dispatch, OSObject* target, void* reference)
{
	IOReturn ret = kIOReturnSuccess;
	if (mDevice == NULL || isInactive())
		ret = kIOReturnNotAttached;
	else if (!mDevice->isOpen(this))
		ret = kIOReturnNotOpen;
	target = this;
	switch (selector) {
		case kPeekMsrUserClientRDMSR:
			ret = methodRDMSR((UInt32*)arguments->structureInput,
							(UInt32*) arguments->structureOutput,
							arguments->structureInputSize,
							(IOByteCount *) &arguments->structureOutputSize);
			break;

		case kPeekMsrUserClientWRMSR:
			ret = methodWRMSR((UInt32*) arguments->structureInput,
							(UInt32*) arguments->structureOutput,
							arguments->structureInputSize,
							(IOByteCount *) &arguments->structureOutputSize);
			break;

		case kPeekMsrUserClientHKern:
			((msrIOdata*)arguments->structureInput)->msr = MSR_IA32_SYSENTER_EIP;//MSR_IA32_LSTAR;
			ret = methodRDMSR((UInt32*) arguments->structureInput,
							(UInt32*) arguments->structureOutput,
							arguments->structureInputSize,
							(IOByteCount *) &arguments->structureOutputSize);

            if ((size_t)(arguments->structureOutputSize) == sizeof(msrIOdata)) {
				printf ("size of output %x\n", arguments->structureOutputSize);
                msrIOdata * out = (msrIOdata *)(arguments->structureOutput);
                try_dump((unsigned long long)(out->param_hi) << 32 | out->param_lo);
            }
            break;

		default:
			ret = kIOReturnBadArgument;		
			break;
	}
	IOLog("external Method %d, %x\n", selector, ret);
	return ret;
}

IOReturn PeekMsrUserClient::methodRDMSR(UInt32 *dataIn, UInt32 *dataOut, IOByteCount inputSize, IOByteCount *outputSize)
{
    msrIOdata * msrindata = (msrIOdata *)dataIn;
    msrIOdata * msroutdata = (msrIOdata *)dataOut;

#ifdef DEBUG
    IOLog("PeekMsr RDMSR called\n");
#endif
    
    if (!dataIn)
        return kIOReturnUnsupported;
	mDevice->a_rdmsr(msrindata->msr, &msrindata->param_lo, &msrindata->param_hi);

#ifdef DEBUG
    IOLog("PeekMsr: RDMSR %x : 0x%08x %08x\n", msrindata->msr, msrindata->param_hi, msrindata->param_lo);
#endif

    if (!dataOut)
        return kIOReturnUnsupported;

	msroutdata->msr = msrindata->msr;
    msroutdata->param_hi = msrindata->param_hi;
    msroutdata->param_lo = msrindata->param_lo;
	*outputSize = sizeof(msrIOdata);

    return kIOReturnSuccess;
}

IOReturn PeekMsrUserClient::methodWRMSR(UInt32 *dataIn, UInt32 *dataOut, IOByteCount inputSize, IOByteCount *outputSize)
{
    msrIOdata * msrindata = (msrIOdata *)dataIn;

#ifdef DEBUG
    IOLog("PeekMsr WRMSR called\n");
#endif

    if (!dataIn)
        return kIOReturnUnsupported;

	mDevice->a_wrmsr(msrindata->msr, msrindata->param_lo, msrindata->param_hi);
    
#ifdef DEBUG
    IOLog("PeekMsr: WRMSR 0x%08x %08x to %x\n", msrindata->param_hi, msrindata->param_lo, msrindata->msr);
#endif
	if (outputSize)
		*outputSize = 0;
    return kIOReturnSuccess;
}
