
#ifndef _MP4_META_H
#define _MP4_META_H

#define __STDC_FORMAT_MACROS
#define __STDC_LIMIT_MACROS

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <unistd.h>
#include <getopt.h>
#include <inttypes.h>

#include <ts/ts.h>

#define MP4_TRAK_ATOM     0
#define MP4_TKHD_ATOM     1
#define MP4_MDIA_ATOM     2
#define MP4_MDHD_ATOM     3
#define MP4_HDLR_ATOM     4
#define MP4_MINF_ATOM     5
#define MP4_VMHD_ATOM     6
#define MP4_SMHD_ATOM     7
#define MP4_DINF_ATOM     8
#define MP4_STBL_ATOM     9
#define MP4_STSD_ATOM    10
#define MP4_STTS_ATOM    11
#define MP4_STTS_DATA    12
#define MP4_STSS_ATOM    13
#define MP4_STSS_DATA    14
#define MP4_CTTS_ATOM    15
#define MP4_CTTS_DATA    16
#define MP4_STSC_ATOM    17
#define MP4_STSC_CHUNK   18
#define MP4_STSC_DATA    19
#define MP4_STSZ_ATOM    20
#define MP4_STSZ_DATA    21
#define MP4_STCO_ATOM    22
#define MP4_STCO_DATA    23
#define MP4_CO64_ATOM    24
#define MP4_CO64_DATA    25
#define MP4_LAST_ATOM    MP4_CO64_DATA

#define MP4_MAX_TRAK_NUM 6

#define MP4_MAX_BUFFER_SIZE         (10 * 1024 * 1024)
#define MP4_MIN_BUFFER_SIZE         1024
#define MP4_MOOV_BUFFER_EXCESS      (4 * 1024)
#define MP4_MAX_OFF_T_VALUE         9223372036854775807LL

#define mp4_set_atom_name(p, n1, n2, n3, n4)                              \
    ((u_char *) (p))[4] = n1;                                             \
    ((u_char *) (p))[5] = n2;                                             \
    ((u_char *) (p))[6] = n3;                                             \
    ((u_char *) (p))[7] = n4

#define mp4_get_32value(p)                                                \
    ( ((uint32_t) ((u_char *) (p))[0] << 24)                              \
    + (           ((u_char *) (p))[1] << 16)                              \
    + (           ((u_char *) (p))[2] << 8)                               \
    + (           ((u_char *) (p))[3]) )

#define mp4_set_32value(p, n)                                             \
    ((u_char *) (p))[0] = (u_char) ((n) >> 24);                           \
    ((u_char *) (p))[1] = (u_char) ((n) >> 16);                           \
    ((u_char *) (p))[2] = (u_char) ((n) >> 8);                            \
    ((u_char *) (p))[3] = (u_char)  (n)

#define mp4_get_64value(p)                                                \
    ( ((uint64_t) ((u_char *) (p))[0] << 56)                              \
    + ((uint64_t) ((u_char *) (p))[1] << 48)                              \
    + ((uint64_t) ((u_char *) (p))[2] << 40)                              \
    + ((uint64_t) ((u_char *) (p))[3] << 32)                              \
    + ((uint64_t) ((u_char *) (p))[4] << 24)                              \
    + (           ((u_char *) (p))[5] << 16)                              \
    + (           ((u_char *) (p))[6] << 8)                               \
    + (           ((u_char *) (p))[7]) )

#define mp4_set_64value(p, n)                                             \
    ((u_char *) (p))[0] = (u_char) ((uint64_t) (n) >> 56);                \
    ((u_char *) (p))[1] = (u_char) ((uint64_t) (n) >> 48);                \
    ((u_char *) (p))[2] = (u_char) ((uint64_t) (n) >> 40);                \
    ((u_char *) (p))[3] = (u_char) ((uint64_t) (n) >> 32);                \
    ((u_char *) (p))[4] = (u_char) (           (n) >> 24);                \
    ((u_char *) (p))[5] = (u_char) (           (n) >> 16);                \
    ((u_char *) (p))[6] = (u_char) (           (n) >> 8);                 \
    ((u_char *) (p))[7] = (u_char)             (n)


typedef struct {
    u_char    size[4];
    u_char    name[4]; 
} mp4_atom_header;

typedef struct {
    u_char    size[4];
    u_char    name[4];
    u_char    size64[8];
} mp4_atom_header64;

typedef struct {
    u_char    size[4];
    u_char    name[4];  
    u_char    version[1];
    u_char    flags[3];
    u_char    creation_time[4];
    u_char    modification_time[4];
    u_char    timescale[4];
    u_char    duration[4];
    u_char    rate[4];
    u_char    volume[2];
    u_char    reserved[10];
    u_char    matrix[36];
    u_char    preview_time[4];
    u_char    preview_duration[4];
    u_char    poster_time[4];
    u_char    selection_time[4];
    u_char    selection_duration[4];
    u_char    current_time[4];
    u_char    next_track_id[4];
} mp4_mvhd_atom;

typedef struct {
    u_char    size[4];
    u_char    name[4];
    u_char    version[1];
    u_char    flags[3];
    u_char    creation_time[8];
    u_char    modification_time[8];
    u_char    timescale[4];
    u_char    duration[8];
    u_char    rate[4];
    u_char    volume[2];
    u_char    reserved[10];
    u_char    matrix[36];
    u_char    preview_time[4];
    u_char    preview_duration[4];
    u_char    poster_time[4];
    u_char    selection_time[4];
    u_char    selection_duration[4];
    u_char    current_time[4];
    u_char    next_track_id[4];
} mp4_mvhd64_atom;

typedef struct {
    u_char    size[4];
    u_char    name[4];
    u_char    version[1];
    u_char    flags[3];
    u_char    creation_time[4];
    u_char    modification_time[4];
    u_char    track_id[4];
    u_char    reserved1[4];
    u_char    duration[4];
    u_char    reserved2[8];
    u_char    layer[2];
    u_char    group[2];
    u_char    volume[2];
    u_char    reverved3[2];
    u_char    matrix[36];
    u_char    width[4];
    u_char    heigth[4];
} mp4_tkhd_atom;

typedef struct {
    u_char    size[4];
    u_char    name[4];
    u_char    version[1];
    u_char    flags[3];
    u_char    creation_time[8];
    u_char    modification_time[8];
    u_char    track_id[4];
    u_char    reserved1[4];
    u_char    duration[8];
    u_char    reserved2[8];
    u_char    layer[2];
    u_char    group[2];
    u_char    volume[2];
    u_char    reverved3[2];
    u_char    matrix[36];
    u_char    width[4];
    u_char    heigth[4];
} mp4_tkhd64_atom;

typedef struct {
    u_char    size[4];
    u_char    name[4];
    u_char    version[1];
    u_char    flags[3];
    u_char    creation_time[4];
    u_char    modification_time[4];
    u_char    timescale[4];
    u_char    duration[4];
    u_char    language[2];
    u_char    quality[2];
} mp4_mdhd_atom;

typedef struct {
    u_char    size[4];
    u_char    name[4];
    u_char    version[1];
    u_char    flags[3];
    u_char    creation_time[8];
    u_char    modification_time[8];
    u_char    timescale[4];
    u_char    duration[8];
    u_char    language[2];
    u_char    quality[2];
} mp4_mdhd64_atom;

typedef struct {
    u_char    size[4];
    u_char    name[4];
    u_char    version[1];
    u_char    flags[3];
    u_char    entries[4];

    u_char    media_size[4];
    u_char    media_name[4];
} mp4_stsd_atom;

typedef struct {
    u_char    size[4];
    u_char    name[4];
    u_char    version[1];
    u_char    flags[3];
    u_char    entries[4];
} mp4_stts_atom;

typedef struct {
    u_char    count[4];
    u_char    duration[4];
} mp4_stts_entry;

typedef struct {
    u_char    size[4];
    u_char    name[4];
    u_char    version[1];
    u_char    flags[3];
    u_char    entries[4];
} mp4_stss_atom;

typedef struct {
    u_char    size[4];
    u_char    name[4];
    u_char    version[1];
    u_char    flags[3];
    u_char    entries[4];
} mp4_ctts_atom;

typedef struct {
    u_char    count[4];
    u_char    offset[4];
} mp4_ctts_entry;

typedef struct {
    u_char    size[4];
    u_char    name[4];
    u_char    version[1];
    u_char    flags[3];
    u_char    entries[4];
} mp4_stsc_atom;

typedef struct {
    u_char    chunk[4];
    u_char    samples[4];
    u_char    id[4];
} mp4_stsc_entry;

typedef struct {
    u_char    size[4];
    u_char    name[4];
    u_char    version[1];
    u_char    flags[3];
    u_char    uniform_size[4];
    u_char    entries[4];
} mp4_stsz_atom;

typedef struct {
    u_char    size[4];
    u_char    name[4];
    u_char    version[1];
    u_char    flags[3];
    u_char    entries[4];
} mp4_stco_atom;

typedef struct {
    u_char    size[4];
    u_char    name[4];
    u_char    version[1];
    u_char    flags[3];
    u_char    entries[4];
} mp4_co64_atom;

class Mp4Meta;
typedef int (Mp4Meta::*Mp4AtomHandler) (int64_t atom_header_size, int64_t atom_data_size);

typedef struct {
    const char      *name;
    Mp4AtomHandler  handler;
} mp4_atom_handler;


class BufferHandle
{
public:
    BufferHandle(): buffer(NULL), reader(NULL)
    {
    }

    ~BufferHandle()
    {
        if (reader) {
            TSIOBufferReaderFree(reader);
            reader = NULL;
        }

        if (buffer) {
            TSIOBufferDestroy(buffer);
            buffer = NULL;
        }
    }

public:
    TSIOBuffer          buffer;
    TSIOBufferReader    reader;
};

struct Mp4Trak
{
    Mp4Trak(): timescale(0), time_to_sample_entries(0), sample_to_chunk_entries(0),
               sync_samples_entries(0), composition_offset_entries(0), sample_sizes_entries(0),
               chunks(0), start_sample(0), start_chunk(0), chunk_samples(0), chunk_samples_size(0),
               start_offset(0), tkhd_size(0), mdhd_size(0), hdlr_size(0), vmhd_size(0), smhd_size(0),
               dinf_size(0), size(0)
    {
        memset(&stsc_chunk_entry, 0, sizeof(mp4_stsc_entry));
    }

    ~Mp4Trak()
    {
    }

    uint32_t        timescale;
    uint32_t        time_to_sample_entries;       // stsc
    uint32_t        sample_to_chunk_entries;      // stsc
    uint32_t        sync_samples_entries;         // stss
    uint32_t        composition_offset_entries;   // ctts
    uint32_t        sample_sizes_entries;         // stsz
    uint32_t        chunks;                       // stco, co64

    uint32_t        start_sample;
    uint32_t        start_chunk;
    uint32_t        chunk_samples;
    uint64_t        chunk_samples_size;
    off_t           start_offset;

    size_t          tkhd_size;
    size_t          mdhd_size;
    size_t          hdlr_size;
    size_t          vmhd_size;
    size_t          smhd_size;
    size_t          dinf_size;
    size_t          size;

    BufferHandle    out[0];
    BufferHandle    trak_atom;
    BufferHandle    tkhd_atom;
    BufferHandle    mdia_atom;
    BufferHandle    mdhd_atom;
    BufferHandle    hdlr_atom;
    BufferHandle    minf_atom;
    BufferHandle    vmhd_atom;
    BufferHandle    smhd_atom;
    BufferHandle    dinf_atom;
    BufferHandle    stbl_atom;            
    BufferHandle    stsd_atom;
    BufferHandle    stts_atom;
    BufferHandle    stts_data;
    BufferHandle    stss_atom;
    BufferHandle    stss_data;
    BufferHandle    ctts_atom;
    BufferHandle    ctts_data;
    BufferHandle    stsc_atom;
    BufferHandle    stsc_chunk;
    BufferHandle    stsc_data;
    BufferHandle    stsz_atom;
    BufferHandle    stsz_data;
    BufferHandle    stco_atom;
    BufferHandle    stco_data;
    BufferHandle    co64_atom;
    BufferHandle    co64_data;

    mp4_stsc_entry  stsc_chunk_entry;
};

class Mp4Meta
{
public:
    Mp4Meta(): start(0), cl(0), content_length(0), meta_atom_size(0),
                  meta_reader(NULL), meta_avail(0), wait_next(0), need_size(0), rs(0), rate(0),
                  ftyp_size(0), moov_size(0), start_pos(0), timescale(0), trak_num(0),
                  offset(0), meta_complete(false)
    {
    }

    ~Mp4Meta()
    {
        uint32_t    i;

        for (i = 0; i < trak_num; i++)
            delete trak_vec[i];
    }

    int parse_meta(bool body_complete);

    int post_process_meta();
    void mp4_meta_consume(int64_t size);
    int mp4_atom_next(int64_t atom_size, bool wait=false);

    int mp4_read_atom(mp4_atom_handler *atom, int64_t size);
    int parse_root_atoms();

    int mp4_read_ftyp_atom(int64_t header_size, int64_t data_size);
    int mp4_read_moov_atom(int64_t header_size, int64_t data_size);
    int mp4_read_mdat_atom(int64_t header_size, int64_t data_size);

    int mp4_read_mvhd_atom(int64_t header_size, int64_t data_size);
    int mp4_read_trak_atom(int64_t header_size, int64_t data_size);
    int mp4_read_cmov_atom(int64_t header_size, int64_t data_size);

    int mp4_read_tkhd_atom(int64_t header_size, int64_t data_size);
    int mp4_read_mdia_atom(int64_t header_size, int64_t data_size);

    int mp4_read_mdhd_atom(int64_t header_size, int64_t data_size);
    int mp4_read_hdlr_atom(int64_t header_size, int64_t data_size);
    int mp4_read_minf_atom(int64_t header_size, int64_t data_size);

    int mp4_read_vmhd_atom(int64_t header_size, int64_t data_size);
    int mp4_read_smhd_atom(int64_t header_size, int64_t data_size);
    int mp4_read_dinf_atom(int64_t header_size, int64_t data_size);
    int mp4_read_stbl_atom(int64_t header_size, int64_t data_size);

    int mp4_read_stsd_atom(int64_t header_size, int64_t data_size);
    int mp4_read_stts_atom(int64_t header_size, int64_t data_size);
    int mp4_read_stss_atom(int64_t header_size, int64_t data_size);
    int mp4_read_ctts_atom(int64_t header_size, int64_t data_size);
    int mp4_read_stsc_atom(int64_t header_size, int64_t data_size);
    int mp4_read_stsz_atom(int64_t header_size, int64_t data_size);
    int mp4_read_stco_atom(int64_t header_size, int64_t data_size);
    int mp4_read_co64_atom(int64_t header_size, int64_t data_size);

    int mp4_update_stts_atom(Mp4Trak *trak);
    int mp4_update_stss_atom(Mp4Trak *trak);
    int mp4_update_ctts_atom(Mp4Trak *trak);
    int mp4_update_stsc_atom(Mp4Trak *trak);
    int mp4_update_stsz_atom(Mp4Trak *trak);
    int mp4_update_co64_atom(Mp4Trak *trak);
    int mp4_update_stco_atom(Mp4Trak *trak);
    int mp4_update_stbl_atom(Mp4Trak *trak);
    int mp4_update_minf_atom(Mp4Trak *trak);
    int mp4_update_mdia_atom(Mp4Trak *trak);
    int mp4_update_trak_atom(Mp4Trak *trak);

    int64_t mp4_update_mdat_atom(int64_t start_offset);
    int mp4_adjust_co64_atom(Mp4Trak *trak, off_t adjustment);
    int mp4_adjust_stco_atom(Mp4Trak *trak, int32_t adjustment);

    uint32_t mp4_find_key_frame(uint32_t start_sample, Mp4Trak *trak);

public:

    int64_t             start;                  // 请求输入的start
    int64_t             cl;                     // 文件本身大小
    int64_t             content_length;         // 重新生成之后的content length
    int64_t             meta_atom_size;

    TSIOBufferReader    meta_reader;            // 传过来的数据, 无需在这里destroy
    int64_t             meta_avail;
    int64_t             wait_next;
    int64_t             need_size;

    BufferHandle        meta_atom;
    BufferHandle        ftyp_atom;
    BufferHandle        moov_atom;
    BufferHandle        mvhd_atom;
    BufferHandle        mdat_atom;
    BufferHandle        mdat_data;
    BufferHandle        out_handle;

    Mp4Trak             *trak_vec[MP4_MAX_TRAK_NUM];

    double              rs;
    double              rate;

    int64_t             ftyp_size;
    int64_t             moov_size;
    int64_t             start_pos;              // 重新生成之后, 剩余的部分的开始位置
    uint32_t            timescale;

    uint32_t            trak_num;

    int64_t             offset;

    u_char              mdat_atom_header[16];

    bool                meta_complete;          // 标识meta已经解析完毕
};

#endif
