/**
* @author:  Egorov Sergey <listrid@yandex.ru>
**/
#include <tdFile.hpp>
#include "tdZip.h"

typedef struct
{
    unsigned short table[16];
    unsigned short trans[288];
} TINF_TREE;

typedef struct
{
    const unsigned char *source;
    const unsigned char *sourceMax;
    unsigned int tag;
    unsigned int bitcount;
    unsigned char *dest;
    unsigned char *destMax;
    unsigned int *destLen;
    TINF_TREE ltree;
    TINF_TREE dtree;
} TINF_DATA;

typedef struct
{
    TINF_TREE sltree;
    TINF_TREE sdtree;
    unsigned char  length_bits[30];
    unsigned short length_base[30];
    unsigned char  dist_bits[30];
    unsigned short dist_base[30];
}gTINF_DATA;
static gTINF_DATA* gTinf = NULL;
static const unsigned char clcidx[] = { 16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15 };

static int tinf_getbit(TINF_DATA *d)
{
    unsigned int bit;
    if (!d->bitcount-- && (size_t)(d->source) <= (size_t)(d->sourceMax))
    {
        d->tag = *d->source++;
        d->bitcount = 7;
    }
    bit = d->tag & 0x01;
    d->tag >>= 1;
    return bit;
}

static unsigned int tinf_read_bits(TINF_DATA *d, int num, int base)
{
    unsigned int val = 0;
    if (num)
    {
        unsigned int limit = 1 << (num);
        unsigned int mask;
        for (mask = 1; mask < limit; mask *= 2)
            if (tinf_getbit(d)) val += mask;
   }
   return val + base;
}

static bool tinf_inflate_uncompressed_block(TINF_DATA *d)
{
    unsigned int length, invlength;
    unsigned int i;
    length = d->source[1];
    length = 256*length + d->source[0];
    invlength = d->source[3];
    invlength = 256*invlength + d->source[2];
    if(length != (~invlength & 0x0000ffff))
        return false;
    d->source += 4;
    if(d->dest)
    {//no test mode
        for (i = length; i; --i)
            *d->dest++ = *d->source++;
    }
    d->bitcount = 0;
    d->destLen[0] += length;
    return true;
}

static int tinf_decode_symbol(TINF_DATA *d, TINF_TREE *t)
{
    int sum = 0, cur = 0, len = 0;
    do{
        cur = 2*cur + tinf_getbit(d);
        ++len;
        sum += t->table[len];
        cur -= t->table[len];
   } while (cur >= 0&& (size_t)(d->source) <= (size_t)(d->sourceMax));
   return t->trans[sum + cur];
}

static bool tinf_inflate_block_data(TINF_DATA *d, TINF_TREE *lt, TINF_TREE *dt)
{
    unsigned char *start = d->dest;
    while (1)
    {
        int sym = tinf_decode_symbol(d, lt);
        if (sym == 256)
        {
            d->destLen[0] += (unsigned int)(d->dest - start);
            return true;
        }
        if (sym < 256)
        {
            if(d->dest)
            {//no test mode
                if((size_t)(d->destMax) <= (size_t)(d->dest))

                    return false;
                *d->dest++ = sym;
            }else{
                d->destLen[0] ++;
            }
      }else{
          int length, dist, offs;
          int i;
          sym -= 257;
          length = tinf_read_bits(d, gTinf->length_bits[sym], gTinf->length_base[sym]);
          dist = tinf_decode_symbol(d, dt);
          offs = tinf_read_bits(d, gTinf->dist_bits[dist], gTinf->dist_base[dist]);
          if(d->dest)
          {//no test mode
              if((size_t)(d->destMax) <= (size_t)(d->dest+length))
                  return false;
              for(i = 0; i < length; ++i)
                  d->dest[i] = d->dest[i - offs];
              d->dest += length;
          }else{
              d->destLen[0] += length;
          }
        }
   }
}

static int tinf_inflate_fixed_block(TINF_DATA *d)
{
    return tinf_inflate_block_data(d, &gTinf->sltree, &gTinf->sdtree);
}

static void tinf_build_tree(TINF_TREE *t, const unsigned char *lengths, unsigned int num)
{
    unsigned short offs[16];
    unsigned int i, sum;
    for (i = 0; i < 16; ++i) t->table[i] = 0;
    for (i = 0; i < num; ++i) t->table[lengths[i]]++;
    t->table[0] = 0;
    for (sum = 0, i = 0; i < 16; ++i)
    {
        offs[i] = sum;
        sum += t->table[i];
    }
    for (i = 0; i < num; ++i)
        if (lengths[i]) t->trans[offs[lengths[i]]++] = i;
}

static void tinf_decode_trees(TINF_DATA *d, TINF_TREE *lt, TINF_TREE *dt)
{
    TINF_TREE code_tree;
    unsigned char lengths[288+32];
    unsigned int hlit, hdist, hclen;
    unsigned int i, num, length;
    hlit = tinf_read_bits(d, 5, 257);
    hdist = tinf_read_bits(d, 5, 1);
    hclen = tinf_read_bits(d, 4, 4);
    for (i = 0; i < 19; ++i)
        lengths[i] = 0;
    for (i = 0; i < hclen; ++i)
        lengths[clcidx[i]] = tinf_read_bits(d, 3, 0);
    tinf_build_tree(&code_tree, lengths, 19);
    for (num = 0; num < hlit + hdist; )
    {
        int sym = tinf_decode_symbol(d, &code_tree);
        switch (sym)
        {
            case 16:
            {
                unsigned char prev = lengths[num - 1];
                for (length = tinf_read_bits(d, 2, 3); length; --length)
                    lengths[num++] = prev;
            }break;
          case 17:
              for (length = tinf_read_bits(d, 3, 3); length; --length)
                  lengths[num++] = 0;
              break;
          case 18:
              for (length = tinf_read_bits(d, 7, 11); length; --length)
                  lengths[num++] = 0;
              break;
          default:lengths[num++] = sym;break;
      }
   }
   tinf_build_tree(lt, lengths, hlit);
   tinf_build_tree(dt, lengths + hlit, hdist);
}

static int tinf_inflate_dynamic_block(TINF_DATA *d)
{
    tinf_decode_trees(d, &d->ltree, &d->dtree);
    return tinf_inflate_block_data(d, &d->ltree, &d->dtree);
}

static void tinf_init();
bool tdInflate(const void *source, unsigned int sourceLen, void *dest, unsigned int *destLen, unsigned int *realSizeData)
{
    TINF_DATA d;
    int bfinal;
    d.source = (const unsigned char *)source;
    d.sourceMax = (const unsigned char *)source+sourceLen;
    d.bitcount = 0;
    d.dest    = (unsigned char *)dest;
    d.destMax = (unsigned char *)dest+*destLen;
    
    d.destLen = destLen;
    *destLen = 0;
    if(gTinf == NULL || gTinf->dist_base[0] == 0)
        tinf_init();
    do{
        unsigned int btype;
        int res;
        bfinal = tinf_getbit(&d);
        btype = tinf_read_bits(&d, 2, 0);
        switch (btype)
        {
            case 0: res = tinf_inflate_uncompressed_block(&d);break;
            case 1: res = tinf_inflate_fixed_block(&d); break;
            case 2: res = tinf_inflate_dynamic_block(&d); break;
            default: return false;
        }
        if(!res)
            return false;
   } while (!bfinal && (size_t)d.source<=(size_t)d.sourceMax);
   if(realSizeData)
       *realSizeData = (unsigned int)((size_t)d.source - (size_t)source);
   return true;
}

bool tdInflate_Test(const void *source, unsigned int sourceLen, unsigned int *destLen, unsigned int *realSizeData)
{
    *destLen = 0;
    return tdInflate(source, sourceLen, NULL, destLen, realSizeData);
}


static void tinf_build_fixed_trees(TINF_TREE *lt, TINF_TREE *dt)
{
    int i;
    for (i = 0; i < 7; ++i)
        lt->table[i] = 0;
    lt->table[7] = 24;
    lt->table[8] = 152;
    lt->table[9] = 112;
    for (i = 0; i < 24; ++i)
        lt->trans[i] = 256 + i;
    for (i = 0; i < 144; ++i)
        lt->trans[24 + i] = i;
    for (i = 0; i < 8; ++i)
        lt->trans[24 + 144 + i] = 280 + i;
    for (i = 0; i < 112; ++i)
        lt->trans[24 + 144 + 8 + i] = 144 + i;
    for (i = 0; i < 5; ++i)
        dt->table[i] = 0;
    dt->table[5] = 32;
    for (i = 0; i < 32; ++i)
        dt->trans[i] = i;
}

static void tinf_build_bits_base(unsigned char *bits, unsigned short *base, int delta, int first)
{
    int i, sum;
    for (i = 0; i < delta; ++i)
        bits[i] = 0;
    for (i = 0; i < 30 - delta; ++i)
        bits[i + delta] = i / delta;
    for (sum = first, i = 0; i < 30; ++i)
    {
        base[i] = sum;
        sum += 1 << bits[i];
    }
}

static void tinf_init()
{
    if(!gTinf)
        gTinf = (gTINF_DATA*)malloc(sizeof(gTINF_DATA));
    tinf_build_fixed_trees(&gTinf->sltree, &gTinf->sdtree);
    tinf_build_bits_base(gTinf->length_bits, gTinf->length_base, 4, 3);
    tinf_build_bits_base(gTinf->dist_bits, gTinf->dist_base, 2, 1);
    gTinf->length_bits[28] = 0;
    gTinf->length_base[28] = 258;
}


unsigned int tdAdler32(const unsigned char *ptr, unsigned int buf_len, unsigned int adler32)
{
    unsigned int i, s1 = adler32 & 0xffff, s2 = adler32 >> 16; size_t block_len = buf_len % 5552;
    while(buf_len)
    {
        for (i = 0; i + 7 < block_len; i += 8, ptr += 8)
        {
            s1 += ptr[0], s2 += s1;
            s1 += ptr[1], s2 += s1;
            s1 += ptr[2], s2 += s1;
            s1 += ptr[3], s2 += s1; 
            s1 += ptr[4], s2 += s1;
            s1 += ptr[5], s2 += s1;
            s1 += ptr[6], s2 += s1;
            s1 += ptr[7], s2 += s1; 
        }
        for(; i < block_len; ++i)
            s1 += *ptr++, s2 += s1;
        s1 %= 65521U, s2 %= 65521U;
        buf_len -= (unsigned int)block_len;
        block_len = 5552;
    }
    return (s2 << 16) + s1;
}


bool tdZlibUncompress(const void *source, unsigned int sourceLen, void *dest, unsigned int *destLen)
{
    unsigned char *src = (unsigned char *)source;
    unsigned char *dst = (unsigned char *)dest;
    unsigned int a32, sizeData;
    unsigned char cmf, flg;
    cmf = src[0];
    flg = src[1];
    
    if ((256*cmf + flg) % 31) return false;
    if ((cmf & 0x0f) != 8) return false;
    if ((cmf >> 4) > 7) return false;
    if (flg & 0x20) return false;
    destLen[0] ++;//fix
    if(!tdInflate(src + 2, sourceLen - 6, dst, destLen, &sizeData))
        return false;
    sizeData += 2;
    a32 =           src[sizeData + 0];
    a32 = 256*a32 + src[sizeData + 1];
    a32 = 256*a32 + src[sizeData + 2];
    a32 = 256*a32 + src[sizeData + 3];
//    if(realSizeData)
//        *realSizeData = sizeData + 4;
    if(a32 != tdAdler32(dst, *destLen, 1))
        return false;
    return true;
}


#pragma pack(2)
struct tdZipReader::tdZipLocalHeader
{
    enum{ SIGNATURE = 0x04034b50, COMP_STORE  = 0, COMP_DEFLAT = 8 };
    uint32_t sig;
    uint16_t version;
    uint16_t flag;
    uint16_t compression;      // COMP_xxxx
    uint16_t modTime;
    uint16_t modDate;
    uint32_t crc32;
    uint32_t cSize;
    uint32_t ucSize;
    uint16_t fnameLen;         // Filename string follows header.
    uint16_t xtraLen;          // Extra field follows filename.
};

struct tdZipReader::tdZipDirHeader
{
    enum { SIGNATURE = 0x06054b50 };
    uint32_t sig;
    uint16_t nDisk;
    uint16_t nStartDisk;
    uint16_t nDirEntries;
    uint16_t totalDirEntries;
    uint32_t dirSize;
    uint32_t dirOffset;
    uint16_t cmntLen;
};

struct tdZipReader::tdZipDirFileHeader
{
    enum { SIGNATURE   = 0x02014b50, COMP_STORE  = 0, COMP_DEFLAT = 8 };
    uint32_t sig;
    uint16_t verMade;
    uint16_t verNeeded;
    uint16_t flag;
    uint16_t compression;      // COMP_xxxx
    uint16_t modTime;
    uint16_t modDate;
    uint32_t crc32;
    uint32_t cSize;            // Compressed size
    uint32_t ucSize;           // Uncompressed size
    uint16_t fnameLen;         // Filename string follows header.
    uint16_t xtraLen;          // Extra field follows filename.
    uint16_t cmntLen;          // Comment field follows extra field.
    uint16_t diskStart;
    uint16_t intAttr;
    uint32_t extAttr;
    uint32_t hdrOffset;

    inline char *GetName   () const { return (char*)(this + 1);   }
    inline char *GetExtra  () const { return GetName() + fnameLen; }
    inline char *GetComment() const { return GetExtra() + xtraLen; }
};

#pragma pack()

tdZipReader::tdZipReader()
{
    m_nEntries = 0;
    m_file = (tdFile*)malloc(sizeof(tdFile));
    m_file->CleanHandle();
}

tdZipReader::~tdZipReader()
{
    Close();
    free(m_file);
}

bool tdZipReader::Parser()
{
    if(!m_file->IsOpen())
        return false;
    tdZipDirHeader dh;
    memset(&dh, 0, sizeof(dh));
    int64_t dhOffset = (m_file->Size() - sizeof(dh));
    m_file->SetPos(dhOffset);
    if(m_file->Read(&dh, sizeof(dh)) != sizeof(dh) || dh.sig != tdZipDirHeader::SIGNATURE)
        return false;
    m_file->SetPos(dhOffset - dh.dirSize);

    m_pDirData = new char[dh.dirSize + dh.nDirEntries*sizeof(*m_papDir)];
    if(!m_pDirData)
        return false;
    memset(m_pDirData, 0, dh.dirSize + dh.nDirEntries*sizeof(*m_papDir));
    m_file->Read(m_pDirData, dh.dirSize);

    char *pfh = m_pDirData;
    m_papDir = (const tdZipDirFileHeader **)(m_pDirData + dh.dirSize);
    bool ret = true;

    for(int i = 0; i < dh.nDirEntries && ret; i++)
    {
        tdZipDirFileHeader &fh = *(tdZipDirFileHeader*)pfh;
        m_papDir[i] = &fh;
        if(fh.sig != tdZipDirFileHeader::SIGNATURE)
        {
            ret = false;
        }else{
            pfh += sizeof(fh);
            for(int j = 0; j < fh.fnameLen; j++)
            {
                if(pfh[j] == '\\')
                    pfh[j] = '/';
            }
            // Skip name, extra and comment fields.
            pfh += fh.fnameLen + fh.xtraLen + fh.cmntLen;
        }
    }
    if(!ret)
    {
        delete[] m_pDirData;
    }else{
        m_nEntries = dh.nDirEntries;
    }
    return ret;
}

bool tdZipReader::Open(const char* fileName)
{
    Close();
    if(!m_file->Open(fileName, tdFile::_OPEN_READ))
        return false;
    if(!Parser())
    {
        m_file->Close();
        return false;
    }
    return true;
}

bool tdZipReader::Open(const wchar_t* fileName)
{
    Close();
    if(!m_file->Open(fileName, tdFile::_OPEN_READ))
        return false;
    if(!Parser())
    {
        m_file->Close();
        return false;
    }
    return true;
}

bool tdZipReader::IsOpen() const
{
    return m_file->IsOpen();
}

void tdZipReader::Close()
{
    if(IsOpen())
    {
        delete[] m_pDirData;
        m_nEntries = 0;
        m_file->Close();
    }
}

int tdZipReader::GetFileID(const char* fileName)
{
    size_t lenIn = strlen(fileName);
    for(int i = 0; i < m_nEntries; i++)
    {
        if(lenIn == m_papDir[i]->fnameLen && !memcmp(fileName, m_papDir[i]->GetName(), lenIn))
            return i;
    }
    return -1;
}

bool tdZipReader::GetFileName(int fileID, char *outName300)  const
{
    if(outName300 == NULL || fileID < 0 || fileID >= m_nEntries)
        return false;
    int len = m_papDir[fileID]->fnameLen;
    if(len > 299)
        len = 299;
    memcpy(outName300, m_papDir[fileID]->GetName(), len);
    outName300[len] = '\0';
    return true;
}

int tdZipReader::GetFileSize(int fileID) const
{
    if(fileID < 0 || fileID >= m_nEntries)
        return -1;
    else
        return m_papDir[fileID]->ucSize;
}


bool tdZipReader::ReadFile(int fileID, void *pBuf)
{
    if(pBuf == NULL || fileID < 0 || fileID >= m_nEntries)
        return false;
    m_file->SetPos(m_papDir[fileID]->hdrOffset);
    tdZipLocalHeader h;
    memset(&h, 0, sizeof(h));
    if(m_file->Read(&h, sizeof(h)) != sizeof(h) || h.sig != tdZipLocalHeader::SIGNATURE)
        return false;
    m_file->SetPos(m_papDir[fileID]->hdrOffset + sizeof(h) +  h.fnameLen + h.xtraLen);
    if(h.compression == tdZipLocalHeader::COMP_STORE)
        return m_file->Read(pBuf, h.cSize) == h.cSize;
    if (h.compression != tdZipLocalHeader::COMP_DEFLAT)
        return false;
    char *pcData = new char[h.cSize];
    if(!pcData)
        return false;
    memset(pcData, 0, h.cSize);
    bool ret = m_file->Read(pcData, h.cSize) == h.cSize;
    if(ret)
    {
        unsigned int destLen = h.ucSize+1;
        ret = tdInflate(pcData, h.cSize, pBuf, &destLen, NULL);
    }
    delete[] pcData;
    return ret;
}


bool tdZipReader::GetFileInfo(int fileID, FileInfo& info)
{
    if(fileID < 0 || fileID >= m_nEntries)
        return false;
    info.m_name    = m_papDir[fileID]->GetName();
    info.m_nameLen = m_papDir[fileID]->fnameLen;
    info.m_size    = m_papDir[fileID]->ucSize;
    DosDateTimeToFileTime(m_papDir[fileID]->modDate, m_papDir[fileID]->modTime, (FILETIME*)&info.m_time);
    
    return true;
}
