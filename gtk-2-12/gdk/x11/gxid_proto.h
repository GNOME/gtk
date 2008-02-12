#define GXID_CLAIM_DEVICE       1
#define GXID_RELEASE_DEVICE     2

#define GXID_RETURN_OK          0
#define GXID_RETURN_ERROR       -1

typedef struct GxidClaimDevice_ GxidClaimDevice;
typedef struct GxidReleaseDevice_ GxidReleaseDevice;
typedef struct GxidMessageAny_ GxidMessageAny;
typedef union GxidMessage_ GxidMessage;

typedef unsigned long GxidU32;
typedef long GxidI32;

struct GxidClaimDevice_ {
  GxidU32 type;
  GxidU32 length;
  GxidU32 device;
  GxidU32 window;
  GxidU32 exclusive;
};

struct GxidReleaseDevice_ {
  GxidU32 type;
  GxidU32 length;
  GxidU32 device;
  GxidU32 window;
};

struct GxidMessageAny_ {
  GxidU32 type;
  GxidU32 length;
};

union GxidMessage_ {
  GxidMessageAny any;
  GxidClaimDevice claim;
  GxidReleaseDevice release;
};
