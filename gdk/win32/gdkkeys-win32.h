#include <glib.h>
#include <Windows.h>

/* For lookup table VK -> chars */
typedef struct 
{
  int table;
  int index;
} GdkWin32KeymapTableAndIndex;

/* For reverse lookup char -> VKs */
typedef struct
{
  BYTE mod_bits;
  BYTE vk;

  /* Index of next KeyEntry. -1 if there is no next entry. */
  int  next;
} GdkWin32KeymapKeyEntry;

typedef struct
{
  HKL            handle;

  /* Keyboard layout identifier */
  char           name[KL_NAMELENGTH];

  /* Path of the layout DLL */
  char          *file;

  /* Handle of the layout DLL */
  HINSTANCE      lib;

  /* The actual conversion tables provided by the layout DLL.
   *
   * This is a pointer to a KBDTABLES structure. The exact definition
   * of this structure depends on the kernel on which the executable
   * run and can in general only be determined at runtime. That's why
   * we have to use a generic gpointer instead of the actual type here.
   *
   * See comment on GdkWin32KeymapImpl below for more information. */
  gpointer       tables;

  /* VK -> chars lookup table so we don't have to do a linear scan
   * every time we look up a key. */
  GdkWin32KeymapTableAndIndex vk_lookup_table[256];

  /* List of entries for reverse (char ->VKs) lookup. */
  GArray        *key_entries;

  /* Reverse lookup table (char -> VKs). Key: Unichar. Value: int.
   * The value is used to index into the key_entries array. The key_entries
   * array can contain multiple consecutive entries for a given char.
   * The end of the list for the char is marked by a key entry that has
   * mod_bits and vk set to 0xFF. */
  GHashTable    *reverse_lookup_table;

  /* Map level to modbits */
  BYTE           level_to_modbits[256];

  /* Max Number of levels */
  BYTE           max_level;

  /* Maximum possible value of a modbits bitset. */
  BYTE           max_modbit_value;

} GdkWin32KeymapLayoutInfo;

/* Some keyboard driver constants
 * Adapted from ReactOS's kbd.h:
 * See https://github.com/reactos/reactos/blob/master/sdk/include/ndk/kbd.h
 */

/* Modifier bits */
#define KBDBASE        0x00
#define KBDSHIFT       0x01
#define KBDCTRL        0x02
#define KBDALT         0x04
#define KBDKANA        0x08
#define KBDROYA        0x10
#define KBDLOYA        0x20
#define KBDGRPSELTAP   0x80

#define KBDALTGR (KBDCTRL| KBDALT)

/* */
#define SHFT_INVALID 0x0F

/* Char table constants */
#define WCH_NONE 0xF000
#define WCH_DEAD 0xF001
#define WCH_LGTR 0xF002

/* Char table flags */
#define CAPLOK      0x01
#define SGCAPS      0x02
#define CAPLOKALTGR 0x04
#define KANALOK     0x08
#define GRPSELTAP   0x80

/* IMPORTANT:
 *
 * Keyboard layout DLLs are dependent on the host architecture.
 *
 * - 32 bit systems have just one 32 bit DLL in System32.
 * - 64 bit systems contain two versions of each layout DLL: One in System32
 *   for 64-bit applications, and one in SysWOW64 for 32-bit applications.
 *
 * Here comes the tricky part:
 *
 * The 32-bit DLL in SysWOW64 is *not* identical to the DLL you would find
 * on a true 32 bit system, because all the pointers there are declared with
 * the attribute `__ptr64` (which means they are 64 bits wide, but only the 
 * lower 32 bits are used).
 *
 * This leads to the following problems:
 *
 *   (1) GCC does not support `__ptr64`
 *   (2) When compiling the 32-bit library, we need two versions of the same code
 *       and decide at run-time which one to execute, because we can't know at
 *       compile time whether we will be running on a true 32-bit system, or on
 *       WOW64.
 *
 * To solve this problem, we generate code for both cases (see
 * gdkkeys-win32-impl.c + gdkkeys-win32-impl-wow64.c) and encapsulate
 * the resulting functions in a struct of type GdkWin32KeymapImpl,
 * allowing us to select the correct implementation at runtime.
 *
 */

typedef struct
{
  gboolean  (*load_layout_dll)      (const char               *dll,
                                     GdkWin32KeymapLayoutInfo *info);
  void      (*init_vk_lookup_table) (GdkWin32KeymapLayoutInfo *info);
  BYTE      (*keystate_to_modbits)  (GdkWin32KeymapLayoutInfo *info,
                                     const BYTE                keystate[256]);
  BYTE      (*modbits_to_level)     (GdkWin32KeymapLayoutInfo *info,
                                     BYTE                      modbits);
  WCHAR     (*vk_to_char_fuzzy)     (GdkWin32KeymapLayoutInfo *info,
                                     const BYTE                keystate[256],
                                     BYTE                      extra_mod_bits,
                                     BYTE                     *consumed_mod_bits,
                                     gboolean                 *is_dead,
                                     BYTE                      vk);
} GdkWin32KeymapImpl;
