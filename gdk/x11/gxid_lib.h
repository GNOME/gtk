#include "gxid_proto.h"

int _gxid_claim_device(char *host, int port, 
		      GxidU32 device, GxidU32 window, int exclusive);
int _gxid_release_device(char *host, int port, GxidU32 device, 
			GxidU32 window);
