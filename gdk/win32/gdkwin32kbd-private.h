//#define NATIVE_PTR_SIZE 8
//#define VOID void
//#define USHORT unsigned short

/* CONCAT(a,b) : Concatenate two identifiers.
 * 
 * The left argument is "pasted" while the right one is evaluated.
 * 
 * The rationale is that we want to be able to call e.g.
 * `CONCAT(VOID, NATIVE_BIT_SIZE)`, where VOID and NATIVE_BIT_SIZE
 * are in turn macros, and want the result to be `VOID8` or `VOID4`,
 * depending on the value of NATIVE_BIT_SIZE. We don't want `VOID`
 * to be expanded, however.
 *
 * In other words, result we want:
 *   - `VOID8`
 * Results we don't want:
 *   - `void8`
 *   - `VOIDNATIVE_BIT_SIZE`
 *   - `voidNATIVE_BIT_SIZE`.
 *
 * To get the preprocessor to only evaluate the right argument, we use
 * double expansion in conjuction with the trick of appending a dummy
 * __VA_ARGS__.
 */
#define CONCAT_HELPER(a,b) a##b
#define CONCAT(a,b,...) CONCAT_HELPER(a##__VA_ARGS__,b)

/* PTRx represents a pointer native to the operating system running
 * the binary. 
 * I.e. 32 bits on 32 bit Windows and 64 bits on 64 bit Windows.
 * 
 * This is independent of the bitness of the application. So even
 * a 32 bit application will use 64 bit pointers when it is running
 * on a 64 bit OS.
 *
 * The following table relates the host & app bitness to the PTRx
 * pointer size.
 *
 *      OS  32  64 
 *  App +---------
 *  32  |   32  64
 *  64  |   -   64
*/
#define PTRx(type) CONCAT(P##type##x, NATIVE_PTR_SIZE)

#define DEFINE_PTRx(type,...)             \
    typedef union {                       \
        PTR(type##__VA_ARGS__) ptr;       \
        BYTE  xxalign[NATIVE_PTR_SIZE];   \
    } CONCAT(P##type##x, NATIVE_PTR_SIZE)

/* Actual pointer */
#define PTR(type) CONCAT(P##type, NATIVE_PTR_SIZE)

/* Given a type name, declares bitness specific type & pointer,
 * and pointer struct. */
#define DEFINE_TYPENAME(name,...)         \
    CONCAT(name##__VA_ARGS__, NATIVE_PTR_SIZE),        \
    *CONCAT(P##name, NATIVE_PTR_SIZE);    \
    DEFINE_PTRx(name##__VA_ARGS__)

/* Defines bitness-specific function name. */
#define FUNC(name,...)                    \
    CONCAT(name##__VA_ARGS__, NATIVE_PTR_SIZE)

typedef USHORT DEFINE_TYPENAME (USHORT);
typedef VOID   DEFINE_TYPENAME (VOID);

/* Driver definitions 
   Adapted from ReactOS's kbd.h:
   See https://github.com/reactos/reactos/blob/master/sdk/include/ndk/kbd.h */

typedef struct
{
  BYTE                    Vk;
  BYTE                    ModBits;
} DEFINE_TYPENAME (VK_TO_BIT);

typedef struct
{
  PTRx(VK_TO_BIT)         pVkToBit;
  WORD                    wMaxModBits;
  BYTE                    ModNumber[1];
} DEFINE_TYPENAME (MODIFIERS);

typedef struct
{
  BYTE                    Vsc;
  USHORT                  Vk;
} DEFINE_TYPENAME (VSC_VK);

typedef struct
{
  BYTE                    Vk;
  BYTE                    Vsc;
} DEFINE_TYPENAME (VK_VSC);

typedef struct
{
  BYTE                    VirtualKey;
  BYTE                    Attributes;
  WCHAR                   wch[1];
} DEFINE_TYPENAME (VK_TO_WCHARS);

typedef struct
{
  PTRx(VK_TO_WCHARS)      pVkToWchars;
  BYTE                    nModifications;
  BYTE                    cbSize;
  BYTE                    xxalign[NATIVE_PTR_SIZE - 2];
} DEFINE_TYPENAME (VK_TO_WCHAR_TABLE);

typedef struct
{
  DWORD                   dwBoth;
  WCHAR                   wchComposed;
  USHORT                  uFlags;
} DEFINE_TYPENAME (DEADKEY);

typedef struct
{
  PTRx(MODIFIERS)         pCharModifiers;
  PTRx(VK_TO_WCHAR_TABLE) pVkToWcharTable;
  PTRx(DEADKEY)           pDeadKey;
  PTRx(VOID)              pKeyNames;
  PTRx(VOID)              pKeyNamesExt;
  PTRx(VOID)              pKeyNamesDead;
  PTRx(USHORT)            pusVSCtoVK;
  BYTE                    bMaxVSCtoVK;
  PTRx(VSC_VK)            pVSCtoVK_E0;
  PTRx(VSC_VK)            pVSCtoVK_E1;
  DWORD                   fLocaleFlags;
  BYTE                    nLgMaxd;
  BYTE                    cbLgEntry;
  PTRx(VOID)              pLigature;
} DEFINE_TYPENAME (KBDTABLES);

/* End of declarations */

static BYTE
FUNC(keystate_to_modbits) (KeyboardLayoutInfo *info,
		           const BYTE          keystate[256])
{
  BYTE result  = 0;
  PTR(KBDTABLES) tables = (PTR(KBDTABLES)) info->tables;
  PTR(VK_TO_BIT) vk_to_bit = tables->pCharModifiers.ptr->pVkToBit.ptr;
  for (int i = 0; vk_to_bit[i].Vk != 0; ++i)
    if (keystate[vk_to_bit[i].Vk] & 0x80)
      result |= vk_to_bit[i].ModBits;
  return result;
}

static BYTE
FUNC(modbits_to_level) (KeyboardLayoutInfo *info,
		        BYTE                modbits)
{
  PTR(KBDTABLES) tables = (PTR(KBDTABLES)) info->tables;
  PTR(MODIFIERS) modifiers = tables->pCharModifiers.ptr;
  if (modbits > modifiers->wMaxModBits)
    return 0;
  return modifiers->ModNumber[modbits];
}

#define POPCOUNT(b) (!!(b & 0x01) + !!(b & 0x02) + !!(b & 0x04) + !!(b & 0x08) + \
                     !!(b & 0x10) + !!(b & 0x20) + !!(b & 0x40) + !!(b & 0x80))

static WCHAR
FUNC(vk_to_char_fuzzy) (KeyboardLayoutInfo *info,
                        const BYTE          keystate[256],
                        BYTE                extra_mod_bits,
                        BYTE               *consumed_mod_bits,
                        gboolean           *is_dead,
                        BYTE                vk)
{
  PTR(KBDTABLES) tables = (PTR(KBDTABLES)) info->tables;
  PTR(VK_TO_WCHAR_TABLE) wch_tables = tables->pVkToWcharTable.ptr;
  
  int table_index = info->vk_lookup_table[vk].table;
  int entry_index = info->vk_lookup_table[vk].index;

  if (table_index == -1 || entry_index == -1)
    return WCH_NONE;

  PTR(VK_TO_WCHAR_TABLE) wch_table = &wch_tables[table_index];

  int n_levels = wch_table->nModifications;
  int entry_size = wch_table->cbSize;

  PTR(VK_TO_WCHARS) entry = (PTR(VK_TO_WCHARS))
	                    ((PBYTE) wch_table->pVkToWchars.ptr
			     + entry_size*entry_index);

  if (entry->VirtualKey == vk)
    {
      BYTE modbits = FUNC(keystate_to_modbits) (info, keystate);
      modbits |= extra_mod_bits;
              
      if ((entry->Attributes & CAPLOK) && 
          !(modbits & ~KBDSHIFT) &&
	  (keystate[VK_CAPITAL] & 0x01))
        modbits ^= KBDSHIFT;
              
      if ((entry->Attributes & CAPLOKALTGR) &&
          (modbits & KBDALTGR) &&
	  (keystate[VK_CAPITAL] & 0x01))
        modbits ^= KBDSHIFT;

      if ((entry->Attributes & SGCAPS) &&
          (keystate[VK_CAPITAL] & 0x01))
        modbits ^= 2;

      if ((entry->Attributes & KANALOK) &&
          (keystate[VK_KANA] & 0x01))
        modbits ^= KBDKANA;

      /* We try to find the entry with the most matching modifiers */
      WCHAR    best_char = WCH_NONE;
      BYTE     best_modifiers = 0;
      int      best_score = -1;
      gboolean best_is_dead = FALSE;
      for (int level = 0; level < n_levels; ++level)
        {
          BYTE candidate_modbits = info->level_to_modbits[level];
          if (candidate_modbits & ~modbits)
              continue;
          gboolean candidate_is_dead = FALSE;
          WCHAR c = entry->wch[level];
          if (c == WCH_DEAD)
            {
              /* Next entry contains the undead keys */
              PTR(VK_TO_WCHARS) next_entry;
              next_entry = (PTR(VK_TO_WCHARS)) ((PBYTE) wch_table->pVkToWchars.ptr
		                                + entry_size * (entry_index + 1));
              c = next_entry->wch[level];
              candidate_is_dead = TRUE;
            }
          if (c == WCH_DEAD || c == WCH_LGTR || c == WCH_NONE)
            continue;

          int score = POPCOUNT (candidate_modbits & modbits);
          if (score > best_score)
            {
              best_score = score;
              best_char = c;
              best_modifiers = candidate_modbits;
              best_is_dead = candidate_is_dead;
            }
        }

      if (consumed_mod_bits)
        *consumed_mod_bits = best_modifiers;

      if (is_dead)
        *is_dead = best_is_dead;

      return best_char;
    }

  return WCH_NONE;
}

static void
FUNC(init_vk_lookup_table) (KeyboardLayoutInfo *info)
{

  memset (info->vk_lookup_table, -1, sizeof (info->vk_lookup_table));
  
  PTR(KBDTABLES) tables = (PTR(KBDTABLES)) info->tables;
  PTR(VK_TO_WCHAR_TABLE) wch_tables = tables->pVkToWcharTable.ptr;
  
  /* Initialize level -> modbits lookup table */
  memset (info->level_to_modbits, 0, sizeof(info->level_to_modbits));
  info->max_level = 0;
  PTR(MODIFIERS) modifiers = tables->pCharModifiers.ptr;
  for (int i = 0; i <= modifiers->wMaxModBits; ++i)
    {
      if (modifiers->ModNumber[i] != SHFT_INVALID &&
          modifiers->ModNumber[i] != 0 /* Workaround for buggy layouts*/)
        {
          if (modifiers->ModNumber[i] > info->max_level)
              info->max_level = modifiers->ModNumber[i];
          info->level_to_modbits[modifiers->ModNumber[i]] = i;
        }
    }

  info->max_modbit_value = modifiers->wMaxModBits;

  /* For convenience, we add 256 identity-mapped entries corresponding to the VKs.
   * This allows us to return a pointer to them from the `gdk_keysym_to_key_entry` function.
   */

  for (int vk = 0; vk < 256; ++vk)
    {
      KeyEntry key_entry = {0};
      key_entry.vk       = vk;
      key_entry.mod_bits = 0;
      key_entry.next     = -1;
      g_array_append_val (info->key_entries, key_entry);
    }

  /* Special entry for ISO_Left_Tab */
  {
    KeyEntry key_entry = {0};
    key_entry.vk       = VK_TAB;
    key_entry.mod_bits = KBDSHIFT;
    key_entry.next     = -1;
    g_array_append_val (info->key_entries, key_entry);
  }

  /* Initialize generic vk <-> char tables */

  for (int table_idx = 0; ; ++table_idx)
    {
      PTR(VK_TO_WCHAR_TABLE) wch_table = &wch_tables[table_idx];
      if (wch_table->pVkToWchars.ptr == NULL)
        break;

      int entry_size = wch_table->cbSize;
      int n_levels   = wch_table->nModifications;

      for (int entry_idx = 0; ; ++entry_idx)
        {
          PTR(VK_TO_WCHARS) entry = (PTR(VK_TO_WCHARS))
		                    ((PBYTE) wch_table->pVkToWchars.ptr
				     + entry_size * entry_idx);

          if (entry->VirtualKey == 0)
            break;

          /* Lookup table to find entry for a VK in O(1). */
          
          info->vk_lookup_table[entry->VirtualKey].table = table_idx;
          info->vk_lookup_table[entry->VirtualKey].index = entry_idx;

          /* Create reverse lookup entries to find a VK+modifier combinations
           * that results in a given character. */
          for (int level = 0; level < n_levels; ++level)
            {
              KeyEntry key_entry = {0};
              key_entry.vk       = entry->VirtualKey;
              key_entry.mod_bits = info->level_to_modbits[level];

              WCHAR c = entry->wch[level];
          
              /* There can be multiple combinations that produce the same character.
               * We store all of them in a linked list.
               * Check if we already have an entry for the character, so we can chain
               * them together. */
              if (!g_hash_table_lookup_extended (info->reverse_lookup_table,
                                                 GINT_TO_POINTER (c),
                                                 NULL, (gpointer*) &key_entry.next))
                {
                  key_entry.next = -1;
                }

              /* We store the KeyEntry in an array. In the hash table we only store
               * the index. */

              g_array_append_val (info->key_entries, key_entry);
              int inserted_idx = info->key_entries->len - 1;

              g_hash_table_insert (info->reverse_lookup_table,
                                   GINT_TO_POINTER (c),
                                   GINT_TO_POINTER (inserted_idx));
          }
        }
    }
}

static gboolean
FUNC(load_layout_dll) (const char         *dll,
		       KeyboardLayoutInfo *info)
{
    typedef PTRx(KBDTABLES) (*KbdLayerDescriptor)(VOID);

    HMODULE            lib;
    KbdLayerDescriptor func;
    PTRx(KBDTABLES)    tables;

    lib = LoadLibraryA (dll);
    if (lib == NULL)
        goto fail1;

    func = (KbdLayerDescriptor) GetProcAddress (lib, "KbdLayerDescriptor");
    if (func == NULL)
        goto fail2;
    
    tables = func();

    info->lib = lib;
    info->tables = (PKBDTABLES) tables.ptr;

    return TRUE;

fail2:
    FreeLibrary (lib);
fail1:
    return FALSE;
}

static KbdFuncs FUNC (kbdfuncs) =
  {
    .load_layout_dll      = FUNC (load_layout_dll),
    .init_vk_lookup_table = FUNC (init_vk_lookup_table),
    .keystate_to_modbits  = FUNC (keystate_to_modbits),
    .modbits_to_level     = FUNC (modbits_to_level),
    .vk_to_char_fuzzy     = FUNC (vk_to_char_fuzzy)
  };
