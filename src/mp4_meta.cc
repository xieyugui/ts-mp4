
#include "mp4_common.h"


static mp4_atom_handler mp4_atoms[] = {
    { "ftyp", &Mp4Meta::mp4_read_ftyp_atom },
    { "moov", &Mp4Meta::mp4_read_moov_atom },
    { "mdat", &Mp4Meta::mp4_read_mdat_atom },
    { NULL, NULL }
};

static mp4_atom_handler mp4_moov_atoms[] = {
    { "mvhd", &Mp4Meta::mp4_read_mvhd_atom },
    { "trak", &Mp4Meta::mp4_read_trak_atom },
    { "cmov", &Mp4Meta::mp4_read_cmov_atom },
    { NULL, NULL }
};

static mp4_atom_handler mp4_trak_atoms[] = {
    { "tkhd", &Mp4Meta::mp4_read_tkhd_atom },
    { "mdia", &Mp4Meta::mp4_read_mdia_atom },
    { NULL, NULL }
};      
    
static mp4_atom_handler mp4_mdia_atoms[] = {
    { "mdhd", &Mp4Meta::mp4_read_mdhd_atom },
    { "hdlr", &Mp4Meta::mp4_read_hdlr_atom },
    { "minf", &Mp4Meta::mp4_read_minf_atom },
    { NULL, NULL }
};

static mp4_atom_handler mp4_minf_atoms[] = {
    { "vmhd", &Mp4Meta::mp4_read_vmhd_atom },
    { "smhd", &Mp4Meta::mp4_read_smhd_atom },
    { "dinf", &Mp4Meta::mp4_read_dinf_atom },
    { "stbl", &Mp4Meta::mp4_read_stbl_atom },
    { NULL, NULL }
};

static mp4_atom_handler mp4_stbl_atoms[] = {
    { "stsd", &Mp4Meta::mp4_read_stsd_atom },
    { "stts", &Mp4Meta::mp4_read_stts_atom },
    { "stss", &Mp4Meta::mp4_read_stss_atom },
    { "ctts", &Mp4Meta::mp4_read_ctts_atom },
    { "stsc", &Mp4Meta::mp4_read_stsc_atom },
    { "stsz", &Mp4Meta::mp4_read_stsz_atom },
    { "stco", &Mp4Meta::mp4_read_stco_atom },
    { "co64", &Mp4Meta::mp4_read_co64_atom },
    { NULL, NULL }
};


void mp4_reader_set_32value(TSIOBufferReader readerp, int64_t offset, uint32_t n);
void mp4_reader_set_64value(TSIOBufferReader readerp, int64_t offset, uint64_t n);
uint32_t mp4_reader_get_32value(TSIOBufferReader readerp, int64_t offset);
uint64_t mp4_reader_get_64value(TSIOBufferReader readerp, int64_t offset);


int
Mp4Meta::parse_meta(bool body_complete)
{
    int             ret, rc;
//    int64_t         haveto;

    meta_avail = TSIOBufferReaderAvail(meta_reader);

    if (wait_next && wait_next <= meta_avail) {
        meta_avail -= wait_next;
        TSIOBufferReaderConsume(meta_reader, wait_next);
        wait_next = 0;
    }

//    haveto = need_size > MP4_MIN_BUFFER_SIZE ? need_size : MP4_MIN_BUFFER_SIZE;

    if (meta_avail < MP4_MIN_BUFFER_SIZE && !body_complete)
        return 0;

    ret = this->parse_root_atoms();     // -1代表出错, 0代表还需要数据来继续解析, 1代表完毕

    if (ret < 0) {
        return -1;

    } else if (ret == 0) {

        if (body_complete) {            // 数据传完了, 但是仍然没有解析完, 算出错
            return -1;

        } else {
            return 0;
        }
    }

    rc = this->post_process_meta();
    if (rc != 0) {
        return -1;
    }

    return 1;
}

void
Mp4Meta::mp4_meta_consume(int64_t size)
{
    TSIOBufferReaderConsume(meta_reader, size);
    meta_avail -= size;
}


int
Mp4Meta::post_process_meta()
{
    off_t       start_offset, adjustment;
    uint32_t    i, j;
    int64_t     avail;
    Mp4Trak     *trak;

    if (this->trak_num == 0) {
        return -1;
    }

    if (mdat_atom.buffer == NULL) {
        return -1;
    }

    out_handle.buffer = TSIOBufferCreate();
    out_handle.reader = TSIOBufferReaderAlloc(out_handle.buffer);

    if (ftyp_atom.buffer) {
        TSIOBufferCopy(out_handle.buffer, ftyp_atom.reader,
                       TSIOBufferReaderAvail(ftyp_atom.reader), 0);
    }

    if (moov_atom.buffer) {
        TSIOBufferCopy(out_handle.buffer, moov_atom.reader,
                       TSIOBufferReaderAvail(moov_atom.reader), 0);
    }

    if (mvhd_atom.buffer) {
        avail = TSIOBufferReaderAvail(mvhd_atom.reader);
        TSIOBufferCopy(out_handle.buffer, mvhd_atom.reader, avail, 0);
        this->moov_size += avail;
    }

    start_offset = cl;

    for (i = 0; i < trak_num; i++) {

        trak = trak_vec[i];

        if (mp4_update_stts_atom(trak) != 0) {
            return -1;
        }

        if (mp4_update_stss_atom(trak) != 0) {
            return -1;
        }

        mp4_update_ctts_atom(trak);

        if (mp4_update_stsc_atom(trak) != 0) {
            return -1;
        }

        if (mp4_update_stsz_atom(trak) != 0) {
            return -1;
        }

        if (trak->co64_data.buffer) {

            if (mp4_update_co64_atom(trak) != 0)
                return -1;

        } else if (mp4_update_stco_atom(trak) != 0) {
            return -1;
        }

        mp4_update_stbl_atom(trak);
        mp4_update_minf_atom(trak);
        trak->size += trak->mdhd_size;
        trak->size += trak->hdlr_size;
        mp4_update_mdia_atom(trak);
        trak->size += trak->tkhd_size;
        mp4_update_trak_atom(trak);

        this->moov_size += trak->size;

        if (start_offset > trak->start_offset)
            start_offset = trak->start_offset;

        for (j = 0; j <= MP4_LAST_ATOM; j++) {
            if (trak->out[j].buffer) {
                TSIOBufferCopy(out_handle.buffer, trak->out[j].reader,
                               TSIOBufferReaderAvail(trak->out[j].reader), 0);
            }
        }
    }

    this->moov_size += 8;

    mp4_reader_set_32value(moov_atom.reader, 0, this->moov_size);
    this->content_length += this->moov_size;

    adjustment = this->ftyp_size + this->moov_size +
                 mp4_update_mdat_atom(start_offset) - start_offset;


    TSIOBufferCopy(out_handle.buffer, mdat_atom.reader,
                   TSIOBufferReaderAvail(mdat_atom.reader), 0);

    for (i = 0; i < trak_num; i++) {
        trak = trak_vec[i];

        if (trak->co64_data.buffer) {
            mp4_adjust_co64_atom(trak, adjustment);

        } else {
            mp4_adjust_stco_atom(trak, adjustment);
        }
    }

    return 0;
}


int
Mp4Meta::parse_root_atoms()
{
    int         i, ret, rc;
    int64_t     atom_size, atom_header_size;
    char        buf[64];
    char        *atom_header, *atom_name;

    memset(buf, 0, sizeof(buf));

    for (;;) {

        if (meta_avail < (int64_t)sizeof(uint32_t))
            return 0;

        TSIOBufferReaderCopy(meta_reader, buf, sizeof(mp4_atom_header64));
        atom_size = mp4_get_32value(buf);

        if (atom_size == 0) {
            // TSDebug(DEBUG_TAG, "[%s] mp4 atom end", __FUNCTION__);      // can be here ?
            return 1;
        }

        atom_header = buf;

        if (atom_size < (int64_t)sizeof(mp4_atom_header)) {

            if (atom_size == 1) {                                       // 需要从扩展头中取出长度信息

                if (meta_avail < (int64_t)sizeof(mp4_atom_header64)) {   // 不够扩展atom头的长度
                    return 0;
                }

            } else {
                // TSDebug(DEBUG_TAG, "[%s] mp4 atom is too small: %"PRId64,
                //        __FUNCTION__, atom_size);
                return -1;
            }

            atom_size = mp4_get_64value(atom_header + 8);
            atom_header_size = sizeof(mp4_atom_header64);

        } else {                                                     // 常规atom头

            if (meta_avail < (int64_t)sizeof(mp4_atom_header))       // 不够atom头的长度
                return 0;

            atom_header_size = sizeof(mp4_atom_header);
        }

        atom_name = atom_header + 4;

        if (atom_size + this->offset > this->cl) {                  // 说明数据走过的位置 + 当前atom大小超过文件总长度, 认为有问题
            // TSDebug(DEBUG_TAG, "[%s] mp4 atom is too large: %"PRId64,
            //        __FUNCTION__, atom_size);
            return -1;
        }

        for (i = 0; mp4_atoms[i].name; i++) {
            if (memcmp(atom_name, mp4_atoms[i].name, 4) == 0) {

                ret = (this->*mp4_atoms[i].handler)(atom_header_size, atom_size - atom_header_size);           // -1表示出错, 0 表示还需要数据继续, 1表示解析完毕

                if (ret <= 0) {
                    return ret;

                } else if (meta_complete) {             // 全部解析完毕
                    return 1;
                }

                goto next;
            }
        }

        // 这里表示并不是关注的box
        rc = mp4_atom_next(atom_size, true);            // 0 表示数据不够, 1表示正常通过
        if (rc == 0) {
            return rc;
        }

next:
        continue;
    }

    return 1;
}

int
Mp4Meta::mp4_atom_next(int64_t atom_size, bool wait)
{
    if (meta_avail >= atom_size) {
        TSIOBufferReaderConsume(meta_reader, atom_size);
        meta_avail -= atom_size;
        return 1;
    }

    if (wait) {
        wait_next = atom_size;
        return 0;
    }

    return -1;
}

int
Mp4Meta::mp4_read_atom(mp4_atom_handler *atom, int64_t size)           // 返回-1表示错误, 正确返回1
{
    int         i, ret, rc;
    int64_t     atom_size, atom_header_size;
    char        buf[32];
    char        *atom_header, *atom_name;

    if (meta_avail < size)    // 数据不够, 对于第二级以下的atom是有问题的
        return -1;

    while (size > 0) {

        if (meta_avail < (int64_t)sizeof(uint32_t))            // 数据不够, 对于第二级以下的atom是有问题的
            return -1;

        TSIOBufferReaderCopy(meta_reader, buf, sizeof(mp4_atom_header64));
        atom_size = mp4_get_32value(buf);

        if (atom_size == 0) {
            // TSDebug(DEBUG_TAG, "[%s] mp4 atom end", __FUNCTION__);
            return 1;
        }

        atom_header = buf;

        if (atom_size < (int64_t)sizeof(mp4_atom_header)) {

            if (atom_size == 1) {                               // 需要从扩展头中取出长度信息

                if (meta_avail < (int64_t)sizeof(mp4_atom_header64)) {   // 不够扩展atom头的长度
                    return -1;
                }

            } else {
                // TSDebug(DEBUG_TAG, "[%s] mp4 atom is too small: %"PRId64,
                //        __FUNCTION__, atom_size);
                return -1;
            }

            atom_size = mp4_get_64value(atom_header + 8);
            atom_header_size = sizeof(mp4_atom_header64);

        } else {                                        // 常规atom头

            if (meta_avail < (int64_t)sizeof(mp4_atom_header))       // 不够atom头的长度
                return -1;

            atom_header_size = sizeof(mp4_atom_header);
        }

        atom_name = atom_header + 4;

        if (atom_size + this->offset > this->cl) {                      // 这里表示数据走过的位置 + 当前这个box的大小超过文件总长度意味着有问题
            // TSDebug(DEBUG_TAG, "[%s] mp4 atom is too large: %"PRId64,
            //        __FUNCTION__, atom_size);
            return -1;
        }

        for (i = 0; atom[i].name; i++) {
            if (memcmp(atom_name, atom[i].name, 4) == 0) {

                if (meta_avail < atom_size)         // 数据不够
                    return -1;

                ret = (this->*atom[i].handler)(atom_header_size, atom_size - atom_header_size);       // -1表示出错, 0 表示解析完毕

                if (ret < 0) {
                    return ret;
                }

                goto next;
            }
        }

        // 这里说明不是关注的box
        rc = mp4_atom_next(atom_size, false);                        // 0 表示数据不够, 1表示正常通过
        if (rc < 0) {
            return rc;
        }

next:
        size -= atom_size;
        continue;
    }

    return 1;
}

int
Mp4Meta::mp4_read_ftyp_atom(int64_t atom_header_size, int64_t atom_data_size)
{
    int64_t         atom_size;

    if (atom_data_size > MP4_MIN_BUFFER_SIZE)   // ftyp数据区太长
        return -1;

    atom_size = atom_header_size + atom_data_size;

    if (meta_avail < atom_size) {               // 数据不够, 由于是第一级别, 可以等待
        return 0;
    }

    ftyp_atom.buffer = TSIOBufferCreate();
    ftyp_atom.reader = TSIOBufferReaderAlloc(ftyp_atom.buffer);

    TSIOBufferCopy(ftyp_atom.buffer, meta_reader, atom_size, 0);
    mp4_meta_consume(atom_size);

    content_length = atom_size;                 // 从这里开始计算真正要输出的长度大小
    ftyp_size = atom_size;

    return 1;
}

int
Mp4Meta::mp4_read_moov_atom(int64_t atom_header_size, int64_t atom_data_size)
{
    int64_t         atom_size;
    int             ret;

    if (mdat_atom.buffer != NULL)       // 不满足流媒体格式
        return -1;

    atom_size = atom_header_size + atom_data_size;

    if (atom_data_size >= MP4_MAX_BUFFER_SIZE)
        return -1;

    if (meta_avail < atom_size) {       // 数据不够, 第一层级的需要等
        return 0;
    }

    moov_atom.buffer = TSIOBufferCreate();
    moov_atom.reader = TSIOBufferReaderAlloc(moov_atom.buffer);

    TSIOBufferCopy(moov_atom.buffer, meta_reader, atom_header_size, 0);
    mp4_meta_consume(atom_header_size);

    ret = mp4_read_atom(mp4_moov_atoms, atom_data_size);

    return ret;
}

int
Mp4Meta::mp4_read_mvhd_atom(int64_t atom_header_size, int64_t atom_data_size)
{
    int64_t             atom_size;
    uint32_t            timescale;
    uint64_t            duration;
    mp4_mvhd_atom       *mvhd;
    mp4_mvhd64_atom     mvhd64;

    if (sizeof(mp4_mvhd_atom) - 8 > (size_t)atom_data_size)
        return -1;

    TSIOBufferReaderCopy(meta_reader, &mvhd64, sizeof(mp4_mvhd64_atom));
    mvhd = (mp4_mvhd_atom*)&mvhd64;

    if (mvhd->version[0] == 0) {
        timescale = mp4_get_32value(mvhd->timescale);
        duration = mp4_get_32value(mvhd->duration);

    } else {        // 64-bit duration
        timescale = mp4_get_32value(mvhd64.timescale);
        duration = mp4_get_64value(mvhd64.duration);
    }

    this->timescale = timescale;
    duration -= this->start * this->timescale / 1000;

    atom_size = atom_header_size + atom_data_size;

    mvhd_atom.buffer = TSIOBufferCreate();
    mvhd_atom.reader = TSIOBufferReaderAlloc(mvhd_atom.buffer);

    TSIOBufferCopy(mvhd_atom.buffer, meta_reader, atom_size, 0);
    mp4_meta_consume(atom_size);

    // 重新设置duration
    if (mvhd->version[0] == 0) {
        mp4_reader_set_32value(mvhd_atom.reader,
                               offsetof(mp4_mvhd_atom, duration), duration);

    } else {
        mp4_reader_set_64value(mvhd_atom.reader,
                               offsetof(mp4_mvhd64_atom, duration), duration);
    }

    return 1;
}

int
Mp4Meta::mp4_read_trak_atom(int64_t atom_header_size, int64_t atom_data_size)
{
    int             rc;
    Mp4Trak         *trak;

    if (trak_num >= MP4_MAX_TRAK_NUM - 1)       // trak数目太多了
        return -1;

    trak = new Mp4Trak();
    trak_vec[trak_num++] = trak;

    trak->trak_atom.buffer = TSIOBufferCreate();
    trak->trak_atom.reader = TSIOBufferReaderAlloc(trak->trak_atom.buffer);

    TSIOBufferCopy(trak->trak_atom.buffer, meta_reader, atom_header_size, 0);
    mp4_meta_consume(atom_header_size);

    rc = mp4_read_atom(mp4_trak_atoms, atom_data_size);

    return rc;
}

int
Mp4Meta::mp4_read_cmov_atom(int64_t atom_header_size, int64_t atom_data_size)
{
    return -1;
}

int
Mp4Meta::mp4_read_tkhd_atom(int64_t atom_header_size, int64_t atom_data_size)
{
    int64_t             atom_size;
    Mp4Trak             *trak;
    mp4_tkhd_atom       *tkhd_atom;
    mp4_tkhd64_atom     tkhd64_atom;
    int64_t             duration;

    TSIOBufferReaderCopy(meta_reader, &tkhd64_atom, sizeof(mp4_tkhd64_atom));
    tkhd_atom = (mp4_tkhd_atom*)&tkhd64_atom;

    if (tkhd_atom->version[0] == 0) {
        duration = mp4_get_32value(tkhd_atom->duration);

    } else {
        duration = mp4_get_64value(tkhd64_atom.duration);
    }

    duration -= this->start * timescale / 1000;

    atom_size = atom_header_size + atom_data_size;

    trak = trak_vec[trak_num-1];
    trak->tkhd_size = atom_size;

    trak->tkhd_atom.buffer = TSIOBufferCreate();
    trak->tkhd_atom.reader = TSIOBufferReaderAlloc(trak->tkhd_atom.buffer);

    TSIOBufferCopy(trak->tkhd_atom.buffer, meta_reader, atom_size, 0);
    mp4_meta_consume(atom_size);

    mp4_reader_set_32value(trak->tkhd_atom.reader,
                           offsetof(mp4_tkhd_atom, size), atom_size);

    if (tkhd_atom->version[0] == 0) {
        mp4_reader_set_32value(trak->tkhd_atom.reader,
                               offsetof(mp4_tkhd_atom, duration), duration);

    } else {
        mp4_reader_set_64value(trak->tkhd_atom.reader,
                               offsetof(mp4_tkhd64_atom, duration), duration);
    }

    return 1;
}

int
Mp4Meta::mp4_read_mdia_atom(int64_t atom_header_size, int64_t atom_data_size)
{
    int64_t             atom_size;
    Mp4Trak             *trak;

    atom_size = atom_header_size + atom_data_size;
    trak = trak_vec[trak_num-1];

    trak->mdia_atom.buffer = TSIOBufferCreate();
    trak->mdia_atom.reader = TSIOBufferReaderAlloc(trak->mdia_atom.buffer);

    TSIOBufferCopy(trak->mdia_atom.buffer, meta_reader, atom_header_size, 0);
    mp4_meta_consume(atom_header_size);

    return mp4_read_atom(mp4_mdia_atoms, atom_data_size);
}

int
Mp4Meta::mp4_read_mdhd_atom(int64_t atom_header_size, int64_t atom_data_size)
{
    int64_t             atom_size, duration;
    uint32_t            ts;
    Mp4Trak             *trak;
    mp4_mdhd_atom       *mdhd;
    mp4_mdhd64_atom     mdhd64;

    TSIOBufferReaderCopy(meta_reader, &mdhd64, sizeof(mp4_mdhd64_atom));
    mdhd = (mp4_mdhd_atom*)&mdhd64;

    if (mdhd->version[0] == 0) {
        ts = mp4_get_32value(mdhd->timescale);
        duration = mp4_get_32value(mdhd->duration);

    } else {
        ts = mp4_get_32value(mdhd64.timescale);
        duration = mp4_get_64value(mdhd64.duration);
    }

    duration -= (uint64_t) this->start * ts / 1000;
    atom_size = atom_header_size + atom_data_size;

    trak = trak_vec[trak_num-1];
    trak->mdhd_size = atom_size;
    trak->timescale = ts;

    trak->mdhd_atom.buffer = TSIOBufferCreate();
    trak->mdhd_atom.reader = TSIOBufferReaderAlloc(trak->mdhd_atom.buffer);

    TSIOBufferCopy(trak->mdhd_atom.buffer, meta_reader, atom_size, 0);
    mp4_meta_consume(atom_size);

    mp4_reader_set_32value(trak->mdhd_atom.reader,
                           offsetof(mp4_mdhd_atom, size), atom_size);

    if (mdhd->version[0] == 0) {
        mp4_reader_set_32value(trak->mdhd_atom.reader,
                               offsetof(mp4_mdhd_atom, duration), duration);

    } else {
        mp4_reader_set_64value(trak->mdhd_atom.reader,
                               offsetof(mp4_mdhd64_atom, duration), duration);
    }

    return 1;
}

int
Mp4Meta::mp4_read_hdlr_atom(int64_t atom_header_size, int64_t atom_data_size)
{
    int64_t             atom_size;
    Mp4Trak             *trak;

    atom_size = atom_header_size + atom_data_size;

    trak = trak_vec[trak_num-1];
    trak->hdlr_size = atom_size;

    trak->hdlr_atom.buffer = TSIOBufferCreate();
    trak->hdlr_atom.reader = TSIOBufferReaderAlloc(trak->hdlr_atom.buffer);

    TSIOBufferCopy(trak->hdlr_atom.buffer, meta_reader, atom_size, 0);
    mp4_meta_consume(atom_size);

    return 1;
}

int
Mp4Meta::mp4_read_minf_atom(int64_t atom_header_size, int64_t atom_data_size)
{
    Mp4Trak             *trak;

    trak = trak_vec[trak_num-1];

    trak->minf_atom.buffer = TSIOBufferCreate();
    trak->minf_atom.reader = TSIOBufferReaderAlloc(trak->minf_atom.buffer);

    TSIOBufferCopy(trak->minf_atom.buffer, meta_reader, atom_header_size, 0);
    mp4_meta_consume(atom_header_size);

    return mp4_read_atom(mp4_minf_atoms, atom_data_size);
}

int
Mp4Meta::mp4_read_vmhd_atom(int64_t atom_header_size, int64_t atom_data_size)
{
    int64_t             atom_size;
    Mp4Trak             *trak;

    atom_size = atom_data_size + atom_header_size;

    trak = trak_vec[trak_num-1];
    trak->vmhd_size += atom_size;

    trak->vmhd_atom.buffer = TSIOBufferCreate();
    trak->vmhd_atom.reader = TSIOBufferReaderAlloc(trak->vmhd_atom.buffer);

    TSIOBufferCopy(trak->vmhd_atom.buffer, meta_reader, atom_size, 0);
    mp4_meta_consume(atom_size);

    return 1;
}

int
Mp4Meta::mp4_read_smhd_atom(int64_t atom_header_size, int64_t atom_data_size)
{
    int64_t             atom_size;
    Mp4Trak             *trak;

    atom_size = atom_data_size + atom_header_size;

    trak = trak_vec[trak_num-1];
    trak->smhd_size += atom_size;

    trak->smhd_atom.buffer = TSIOBufferCreate();
    trak->smhd_atom.reader = TSIOBufferReaderAlloc(trak->smhd_atom.buffer);

    TSIOBufferCopy(trak->smhd_atom.buffer, meta_reader, atom_size, 0);
    mp4_meta_consume(atom_size);

    return 1;
}

int
Mp4Meta::mp4_read_dinf_atom(int64_t atom_header_size, int64_t atom_data_size)
{
    int64_t             atom_size;
    Mp4Trak             *trak;

    atom_size = atom_data_size + atom_header_size;

    trak = trak_vec[trak_num-1];
    trak->dinf_size += atom_size;

    trak->dinf_atom.buffer = TSIOBufferCreate();
    trak->dinf_atom.reader = TSIOBufferReaderAlloc(trak->dinf_atom.buffer);

    TSIOBufferCopy(trak->dinf_atom.buffer, meta_reader, atom_size, 0);
    mp4_meta_consume(atom_size);

    return 1;
}

int
Mp4Meta::mp4_read_stbl_atom(int64_t atom_header_size, int64_t atom_data_size)
{
    int64_t             atom_size;
    Mp4Trak             *trak;

    atom_size = atom_data_size + atom_header_size;

    trak = trak_vec[trak_num-1];

    trak->stbl_atom.buffer = TSIOBufferCreate();
    trak->stbl_atom.reader = TSIOBufferReaderAlloc(trak->stbl_atom.buffer);

    TSIOBufferCopy(trak->stbl_atom.buffer, meta_reader, atom_header_size, 0);
    mp4_meta_consume(atom_header_size);

    return mp4_read_atom(mp4_stbl_atoms, atom_data_size);
}

int
Mp4Meta::mp4_read_stsd_atom(int64_t atom_header_size, int64_t atom_data_size)
{
    int64_t             atom_size;
    Mp4Trak             *trak;

    atom_size = atom_data_size + atom_header_size;

    trak = trak_vec[trak_num-1];
    trak->size += atom_size;

    trak->stsd_atom.buffer = TSIOBufferCreate();
    trak->stsd_atom.reader = TSIOBufferReaderAlloc(trak->stsd_atom.buffer);

    TSIOBufferCopy(trak->stsd_atom.buffer, meta_reader, atom_size, 0);

    mp4_meta_consume(atom_size);

    return 1;
}

int
Mp4Meta::mp4_read_stts_atom(int64_t atom_header_size, int64_t atom_data_size)
{
    int32_t         entries;
    int64_t         esize;
    mp4_stts_atom   stts;
    Mp4Trak         *trak;

    if (sizeof(mp4_stts_atom) - 8 > (size_t)atom_data_size)
        return -1;

    TSIOBufferReaderCopy(meta_reader, &stts, sizeof(mp4_stts_atom));

    entries = mp4_get_32value(stts.entries);
    esize = entries * sizeof(mp4_stts_entry);

    if (sizeof(mp4_stts_atom) - 8 + esize > (size_t)atom_data_size)
        return -1;

    trak = trak_vec[trak_num - 1];
    trak->time_to_sample_entries = entries;

    trak->stts_atom.buffer = TSIOBufferCreate();
    trak->stts_atom.reader = TSIOBufferReaderAlloc(trak->stts_atom.buffer);
    TSIOBufferCopy(trak->stts_atom.buffer, meta_reader, sizeof(mp4_stts_atom), 0);

    trak->stts_data.buffer = TSIOBufferCreate();
    trak->stts_data.reader = TSIOBufferReaderAlloc(trak->stts_data.buffer);
    TSIOBufferCopy(trak->stts_data.buffer, meta_reader, esize, sizeof(mp4_stts_atom));

    mp4_meta_consume(atom_data_size + atom_header_size);

    return 1;
}

int
Mp4Meta::mp4_read_stss_atom(int64_t atom_header_size, int64_t atom_data_size)
{
    int32_t         entries;
    int64_t         esize;
    mp4_stss_atom   stss;
    Mp4Trak         *trak;

    if (sizeof(mp4_stss_atom) - 8 > (size_t)atom_data_size)
        return -1;

    TSIOBufferReaderCopy(meta_reader, &stss, sizeof(mp4_stss_atom));
    entries = mp4_get_32value(stss.entries);
    esize = entries * sizeof(int32_t);

    if (sizeof(mp4_stss_atom) - 8 + esize > (size_t)atom_data_size)
        return -1;

    trak = trak_vec[trak_num-1];
    trak->sync_samples_entries = entries;

    trak->stss_atom.buffer = TSIOBufferCreate();
    trak->stss_atom.reader = TSIOBufferReaderAlloc(trak->stss_atom.buffer);
    TSIOBufferCopy(trak->stss_atom.buffer, meta_reader, sizeof(mp4_stss_atom), 0);

    trak->stss_data.buffer = TSIOBufferCreate();
    trak->stss_data.reader = TSIOBufferReaderAlloc(trak->stss_data.buffer);
    TSIOBufferCopy(trak->stss_data.buffer, meta_reader, esize, sizeof(mp4_stss_atom));

    mp4_meta_consume(atom_data_size + atom_header_size);

    return 1;
}

int
Mp4Meta::mp4_read_ctts_atom(int64_t atom_header_size, int64_t atom_data_size)
{
    int32_t         entries;
    int64_t         esize;
    mp4_ctts_atom   ctts;
    Mp4Trak         *trak;

    if (sizeof(mp4_ctts_atom) - 8 > (size_t)atom_data_size)
        return -1;

    TSIOBufferReaderCopy(meta_reader, &ctts, sizeof(mp4_ctts_atom));
    entries = mp4_get_32value(ctts.entries);
    esize = entries * sizeof(mp4_ctts_entry);

    if (sizeof(mp4_ctts_atom) - 8 + esize > (size_t)atom_data_size)
        return -1;

    trak = trak_vec[trak_num - 1];
    trak->composition_offset_entries = entries;

    trak->ctts_atom.buffer = TSIOBufferCreate();
    trak->ctts_atom.reader = TSIOBufferReaderAlloc(trak->ctts_atom.buffer);
    TSIOBufferCopy(trak->ctts_atom.buffer, meta_reader, sizeof(mp4_ctts_atom), 0);

    trak->ctts_data.buffer = TSIOBufferCreate();
    trak->ctts_data.reader = TSIOBufferReaderAlloc(trak->ctts_data.buffer);
    TSIOBufferCopy(trak->ctts_data.buffer, meta_reader, esize, sizeof(mp4_ctts_atom));

    mp4_meta_consume(atom_data_size + atom_header_size);

    return 1;
}

int
Mp4Meta::mp4_read_stsc_atom(int64_t atom_header_size, int64_t atom_data_size)
{
    int32_t         entries;
    int64_t         esize;
    mp4_stsc_atom   stsc;
    Mp4Trak         *trak;

    if (sizeof(mp4_stsc_atom) - 8 > (size_t)atom_data_size)
        return -1;

    TSIOBufferReaderCopy(meta_reader, &stsc, sizeof(mp4_stsc_atom));
    entries = mp4_get_32value(stsc.entries);
    esize = entries * sizeof(mp4_stsc_entry);

    if (sizeof(mp4_stsc_atom) - 8 + esize > (size_t)atom_data_size)
        return -1;

    trak = trak_vec[trak_num - 1];
    trak->sample_to_chunk_entries = entries;

    trak->stsc_atom.buffer = TSIOBufferCreate();
    trak->stsc_atom.reader = TSIOBufferReaderAlloc(trak->stsc_atom.buffer);
    TSIOBufferCopy(trak->stsc_atom.buffer, meta_reader, sizeof(mp4_stsc_atom), 0);

    trak->stsc_data.buffer = TSIOBufferCreate();
    trak->stsc_data.reader = TSIOBufferReaderAlloc(trak->stsc_data.buffer);
    TSIOBufferCopy(trak->stsc_data.buffer, meta_reader, esize, sizeof(mp4_stsc_atom));

    mp4_meta_consume(atom_data_size + atom_header_size);

    return 1;
}

int
Mp4Meta::mp4_read_stsz_atom(int64_t atom_header_size, int64_t atom_data_size)
{
    int32_t         entries, size;
    int64_t         esize, atom_size;
    mp4_stsz_atom   stsz;
    Mp4Trak         *trak;

    if (sizeof(mp4_stsz_atom) - 8 > (size_t)atom_data_size)
        return -1;

    TSIOBufferReaderCopy(meta_reader, &stsz, sizeof(mp4_stsz_atom));
    entries = mp4_get_32value(stsz.entries);
    esize = entries * sizeof(int32_t);

    trak = trak_vec[trak_num - 1];
    size = mp4_get_32value(stsz.uniform_size);

    trak->sample_sizes_entries = entries;

    trak->stsz_atom.buffer = TSIOBufferCreate();
    trak->stsz_atom.reader = TSIOBufferReaderAlloc(trak->stsz_atom.buffer);
    TSIOBufferCopy(trak->stsz_atom.buffer, meta_reader, sizeof(mp4_stsz_atom), 0);

    if (size == 0) {

        if (sizeof(mp4_stsz_atom) - 8 + esize > (size_t)atom_data_size)
            return -1;

        trak->stsz_data.buffer = TSIOBufferCreate();
        trak->stsz_data.reader = TSIOBufferReaderAlloc(trak->stsz_data.buffer);
        TSIOBufferCopy(trak->stsz_data.buffer, meta_reader, esize, sizeof(mp4_stsz_atom));

    } else {
        atom_size = atom_header_size + atom_data_size;
        trak->size += atom_size;
        mp4_reader_set_32value(trak->stsz_atom.reader, 0, atom_size);
    }

    mp4_meta_consume(atom_data_size + atom_header_size);

    return 1;
}

int
Mp4Meta::mp4_read_stco_atom(int64_t atom_header_size, int64_t atom_data_size)
{
    int32_t         entries;
    int64_t         esize;
    mp4_stco_atom   stco;
    Mp4Trak         *trak;

    if (sizeof(mp4_stco_atom) - 8 > (size_t)atom_data_size)
        return -1;

    TSIOBufferReaderCopy(meta_reader, &stco, sizeof(mp4_stco_atom));
    entries = mp4_get_32value(stco.entries);
    esize = entries * sizeof(int32_t);

    if (sizeof(mp4_stco_atom) - 8 + esize > (size_t)atom_data_size)
        return -1;

    trak = trak_vec[trak_num - 1];
    trak->chunks = entries;

    trak->stco_atom.buffer = TSIOBufferCreate();
    trak->stco_atom.reader = TSIOBufferReaderAlloc(trak->stco_atom.buffer);
    TSIOBufferCopy(trak->stco_atom.buffer, meta_reader, sizeof(mp4_stco_atom), 0);

    trak->stco_data.buffer = TSIOBufferCreate();
    trak->stco_data.reader = TSIOBufferReaderAlloc(trak->stco_data.buffer);
    TSIOBufferCopy(trak->stco_data.buffer, meta_reader, esize, sizeof(mp4_stco_atom));

    mp4_meta_consume(atom_data_size + atom_header_size);

    return 1;
}

int
Mp4Meta::mp4_read_co64_atom(int64_t atom_header_size, int64_t atom_data_size)
{
    int32_t         entries;
    int64_t         esize;
    mp4_co64_atom   co64;
    Mp4Trak         *trak;

    if (sizeof(mp4_co64_atom) - 8 > (size_t)atom_data_size)
        return -1;

    TSIOBufferReaderCopy(meta_reader, &co64, sizeof(mp4_co64_atom));
    entries = mp4_get_32value(co64.entries);
    esize = entries * sizeof(int64_t);

    if (sizeof(mp4_co64_atom) - 8 + esize > (size_t)atom_data_size)
        return -1;

    trak = trak_vec[trak_num - 1];
    trak->chunks = entries;

    trak->co64_atom.buffer = TSIOBufferCreate();
    trak->co64_atom.reader = TSIOBufferReaderAlloc(trak->co64_atom.buffer);
    TSIOBufferCopy(trak->co64_atom.buffer, meta_reader, sizeof(mp4_co64_atom), 0);

    trak->co64_data.buffer = TSIOBufferCreate();
    trak->co64_data.reader = TSIOBufferReaderAlloc(trak->co64_data.buffer);
    TSIOBufferCopy(trak->co64_data.buffer, meta_reader, esize, sizeof(mp4_co64_atom));

    mp4_meta_consume(atom_data_size + atom_header_size);

    return 1;
}

int
Mp4Meta::mp4_read_mdat_atom(int64_t atom_header_size, int64_t atom_data_size)
{
    int64_t     atom_size;

    atom_size = atom_header_size + atom_data_size;

    mdat_atom.buffer = TSIOBufferCreate();
    mdat_atom.reader = TSIOBufferReaderAlloc(mdat_atom.buffer);

    meta_complete = true;
    return 1;
}

int
Mp4Meta::mp4_update_stts_atom(Mp4Trak *trak)
{
    uint32_t            i, entries, count, duration, pass;
    uint32_t            start_sample, left, start_count, total;
    uint32_t            key_sample, old_sample;
    uint64_t            start_time;
    int64_t             atom_size;
    TSIOBufferReader    readerp;

    if (trak->stts_data.buffer == NULL)
        return -1;

    total = start_count = 0;

    entries = trak->time_to_sample_entries;
    start_time = this->start * trak->timescale / 1000;
    if (this->rs > 0) {
        start_time = (uint64_t)(this->rs * trak->timescale);
    }

    start_sample = 0;
    readerp = TSIOBufferReaderClone(trak->stts_data.reader);

    if (trak->time_to_sample_entries == 1) {            // 后续做进一步处理
        total = (uint32_t)mp4_reader_get_32value(readerp, offsetof(mp4_stts_entry, count));

        if (this->rate > 0) {
            start_count = (uint32_t)(this->rate * total);
        }
    }

    for (i = 0; i < entries; i++) {
        duration = (uint32_t)mp4_reader_get_32value(readerp,
                                    offsetof(mp4_stts_entry, duration));

        count = (uint32_t)mp4_reader_get_32value(readerp,
                                     offsetof(mp4_stts_entry, count));

        if (start_count > 0) {
            if (start_count < count) {
                start_sample = start_count;
                count -= start_count;
                goto found;
            }

        } else if (start_time < (uint64_t)count * duration) {
            pass = (uint32_t)(start_time/duration);
            start_sample += pass;           // 把还要过掉的sample数加上
            count -= pass;                  // 这组sample还要保留的个数

            // 因为还需要查找关键帧, 所以这里先不设置
            // mp4_reader_set_32value(readerp, offsetof(mp4_stts_entry, count), count);

            goto found;
        }

        start_sample += count;
        start_time -= count * duration;
        TSIOBufferReaderConsume(readerp, sizeof(mp4_stts_entry));
    }

    TSIOBufferReaderFree(readerp);

    return -1;

found:

    old_sample = start_sample;
    key_sample = this->mp4_find_key_frame(start_sample, trak);              // 查找关键帧, 关键帧是从1开始计算
    if (old_sample != key_sample) {
        start_sample = key_sample - 1;
        this->rs = (double)start_sample * 1000/(double)trak->timescale;      // 重新计算开始时间

        if (trak->time_to_sample_entries == 1) {
            this->rate = (double)start_sample/(double)total;                 // 计算一个百分比
        }

    } else {                    // 每一帧都是关键帧

    }

    TSIOBufferReaderFree(readerp);
    readerp = TSIOBufferReaderClone(trak->stts_data.reader);        // 重新生成reader

    trak->start_sample = start_sample;

    /* 重新计算count和entry */
    for (i = 0; i < entries; i++) {
        count = (uint32_t)mp4_reader_get_32value(readerp, offsetof(mp4_stts_entry, count));

        if (start_sample < count) {
            count -= start_sample;
            mp4_reader_set_32value(readerp, offsetof(mp4_stts_entry, count), count);
            break;
        }

        start_sample -= count;
    }

    left = entries - i;         // 剩余的条目数

    atom_size = sizeof(mp4_stts_atom) + left * sizeof(mp4_stts_entry);
    trak->size += atom_size;

    mp4_reader_set_32value(trak->stts_atom.reader, offsetof(mp4_stts_atom, size),
                           atom_size);

    mp4_reader_set_32value(trak->stts_atom.reader, offsetof(mp4_stts_atom, entries),
                           left);

    TSIOBufferReaderConsume(trak->stts_data.reader, i * sizeof(mp4_stts_entry));
    TSIOBufferReaderFree(readerp);

    return 0;
}

int
Mp4Meta::mp4_update_stss_atom(Mp4Trak *trak)
{
    int64_t             atom_size;
    uint32_t            i, j, entries, sample, start_sample, left;
    TSIOBufferReader    readerp;

    if (trak->stss_data.buffer == NULL)
        return 0;

    readerp = TSIOBufferReaderClone(trak->stss_data.reader);

    start_sample = trak->start_sample + 1;          // 这里得到的start_sample是关键帧号, 关键帧号是从1开始的
    entries = trak->sync_samples_entries;

    for (i = 0; i < entries ; i++) {
        sample = (uint32_t)mp4_reader_get_32value(readerp, 0);

        if (sample >= start_sample) {
            goto found;
        }

        TSIOBufferReaderConsume(readerp, sizeof(uint32_t));
    }

    TSIOBufferReaderFree(readerp);
    return -1;

found:

    left = entries - i;

    start_sample = trak->start_sample;
    for (j = 0; j < left; j++) {
        sample = (uint32_t)mp4_reader_get_32value(readerp, 0);
        sample -= start_sample;
        mp4_reader_set_32value(readerp, 0, sample);
        TSIOBufferReaderConsume(readerp, sizeof(uint32_t));
    }

    atom_size = sizeof(mp4_stss_atom) + left * sizeof(uint32_t);
    trak->size += atom_size;

    mp4_reader_set_32value(trak->stss_atom.reader, offsetof(mp4_stss_atom, size),
                           atom_size);

    mp4_reader_set_32value(trak->stss_atom.reader, offsetof(mp4_stss_atom, entries),
                           left);

    TSIOBufferReaderConsume(trak->stss_data.reader, i * sizeof(uint32_t));
    TSIOBufferReaderFree(readerp);

    return 0;
}

int
Mp4Meta::mp4_update_ctts_atom(Mp4Trak *trak)
{
    int64_t             atom_size;
    uint32_t            i, entries, start_sample, left;
    uint32_t            count;
    TSIOBufferReader    readerp;

    if (trak->ctts_data.buffer == NULL)
        return 0;

    readerp = TSIOBufferReaderClone(trak->ctts_data.reader);

    start_sample = trak->start_sample + 1;          // 因为关键帧号码是从1开始的
    entries = trak->composition_offset_entries;

    for (i = 0; i < entries; i++) {
        count = (uint32_t)mp4_reader_get_32value(readerp, offsetof(mp4_ctts_entry, count));

        if (start_sample <= count) {
            count -= (start_sample - 1);
            mp4_reader_set_32value(readerp, offsetof(mp4_ctts_entry, count), count);
            goto found;
        }

        start_sample -= count;
        TSIOBufferReaderConsume(readerp, sizeof(mp4_ctts_entry));
    }

    if (trak->ctts_atom.reader) {
        TSIOBufferReaderFree(trak->ctts_atom.reader);
        TSIOBufferDestroy(trak->ctts_atom.buffer);

        trak->ctts_atom.buffer = NULL;
        trak->ctts_atom.reader = NULL;
    }

    if (trak->ctts_data.reader) {
        TSIOBufferReaderFree(trak->ctts_data.reader);
        TSIOBufferDestroy(trak->ctts_data.buffer);

        trak->ctts_data.reader = NULL;
        trak->ctts_data.buffer = NULL;
    }

    TSIOBufferReaderFree(readerp);
    return 0;

found:

    left = entries - i;
    atom_size = sizeof(mp4_ctts_atom) + left * sizeof(mp4_ctts_entry);
    trak->size += atom_size;

    mp4_reader_set_32value(trak->ctts_atom.reader, offsetof(mp4_ctts_atom, size), atom_size);
    mp4_reader_set_32value(trak->ctts_atom.reader, offsetof(mp4_ctts_atom, entries), left);

    TSIOBufferReaderConsume(trak->ctts_data.reader, i * sizeof(mp4_ctts_entry));
    TSIOBufferReaderFree(readerp);

    return 0;
}

int
Mp4Meta::mp4_update_stsc_atom(Mp4Trak *trak)
{
    int64_t             atom_size;
    uint32_t            i, entries, samples, start_sample;
    uint32_t            chunk, next_chunk, n, id, j;
    mp4_stsc_entry      *first;
    TSIOBufferReader    readerp;

    if (trak->stsc_data.buffer == NULL)
        return -1;

    if (trak->sample_to_chunk_entries == 0)
        return -1;

    start_sample = (uint32_t) trak->start_sample;
    entries = trak->sample_to_chunk_entries - 1;

    readerp = TSIOBufferReaderClone(trak->stsc_data.reader);

    chunk = mp4_reader_get_32value(readerp, offsetof(mp4_stsc_entry, chunk));
    samples = mp4_reader_get_32value(readerp, offsetof(mp4_stsc_entry, samples));
    id = mp4_reader_get_32value(readerp, offsetof(mp4_stsc_entry, id));

    TSIOBufferReaderConsume(readerp, sizeof(mp4_stsc_entry));

    for (i = 1; i < trak->sample_to_chunk_entries; i++) {
        next_chunk = mp4_reader_get_32value(readerp, offsetof(mp4_stsc_entry, chunk));

        n = (next_chunk - chunk) * samples;

        if (start_sample <= n) {
            goto found;
        }

        start_sample -= n;

        chunk = next_chunk;
        samples = mp4_reader_get_32value(readerp, offsetof(mp4_stsc_entry, samples));
        id = mp4_reader_get_32value(readerp, offsetof(mp4_stsc_entry, id));

        TSIOBufferReaderConsume(readerp, sizeof(mp4_stsc_entry));
    }

    next_chunk = trak->chunks;

    n = (next_chunk - chunk) * samples;
    if (start_sample > n) {
        TSIOBufferReaderFree(readerp);
        return -1;
    }

found:

    TSIOBufferReaderFree(readerp);

    entries = trak->sample_to_chunk_entries - i + 1;
    if (samples == 0)
        return -1;

    // 重新生成readerp
    readerp = TSIOBufferReaderClone(trak->stsc_data.reader);
    TSIOBufferReaderConsume(readerp, sizeof(mp4_stsc_entry) * (i-1));

    trak->start_chunk = chunk - 1;
    trak->start_chunk += start_sample / samples;
    trak->chunk_samples = start_sample % samples;

    atom_size = sizeof(mp4_stsc_atom) + entries * sizeof(mp4_stsc_entry);

    mp4_reader_set_32value(readerp, offsetof(mp4_stsc_entry, chunk), 1);

    if (trak->chunk_samples && next_chunk - trak->start_chunk == 2) {
        mp4_reader_set_32value(readerp, offsetof(mp4_stsc_entry, samples),
                               samples - trak->chunk_samples);

    } else if (trak->chunk_samples) {
        first = &trak->stsc_chunk_entry;
        mp4_set_32value(first->chunk, 1);
        mp4_set_32value(first->samples, samples - trak->chunk_samples);
        mp4_set_32value(first->id, id);

        trak->stsc_chunk.buffer = TSIOBufferSizedCreate(TS_IOBUFFER_SIZE_INDEX_128);
        trak->stsc_chunk.reader = TSIOBufferReaderAlloc(trak->stsc_chunk.buffer);
        TSIOBufferWrite(trak->stsc_chunk.buffer, first, sizeof(mp4_stsc_entry));

        mp4_reader_set_32value(readerp, offsetof(mp4_stsc_entry, chunk), 2);

        entries++;
        atom_size += sizeof(mp4_stsc_entry);
    }

    TSIOBufferReaderConsume(readerp, sizeof(mp4_stsc_entry));

    for (j = i; j < trak->sample_to_chunk_entries; j++) {
        chunk = mp4_reader_get_32value(readerp, offsetof(mp4_stsc_entry, chunk));
        chunk -= trak->start_chunk;
        mp4_reader_set_32value(readerp, offsetof(mp4_stsc_entry, chunk), chunk);
        TSIOBufferReaderConsume(readerp, sizeof(mp4_stsc_entry));
    }

    trak->size += atom_size;

    mp4_reader_set_32value(trak->stsc_atom.reader, offsetof(mp4_stsc_atom, size), atom_size);
    mp4_reader_set_32value(trak->stsc_atom.reader, offsetof(mp4_stsc_atom, entries), entries);

    TSIOBufferReaderConsume(trak->stsc_data.reader, (i-1) * sizeof(mp4_stsc_entry));
    TSIOBufferReaderFree(readerp);

    return 0;
}

int
Mp4Meta::mp4_update_stsz_atom(Mp4Trak *trak)
{
    uint32_t            i;
    int64_t             atom_size, avail;
    uint32_t            pass;
    TSIOBufferReader    readerp;

    if (trak->stsz_data.buffer == NULL)
        return 0;

    if (trak->start_sample > trak->sample_sizes_entries)
        return -1;

    readerp = TSIOBufferReaderClone(trak->stsz_data.reader);
    avail = TSIOBufferReaderAvail(readerp);

    pass = trak->start_sample * sizeof(uint32_t);

    TSIOBufferReaderConsume(readerp, pass - sizeof(uint32_t)*(trak->chunk_samples));

    for (i = 0; i < trak->chunk_samples; i++) {
        trak->chunk_samples_size += mp4_reader_get_32value(readerp, 0);
        TSIOBufferReaderConsume(readerp, sizeof(uint32_t));
    }

    atom_size = sizeof(mp4_stsz_atom) + avail - pass;
    trak->size += atom_size;

    mp4_reader_set_32value(trak->stsz_atom.reader, offsetof(mp4_stsz_atom, size),
                           atom_size);
    mp4_reader_set_32value(trak->stsz_atom.reader, offsetof(mp4_stsz_atom, entries),
                           trak->sample_sizes_entries - trak->start_sample);

    TSIOBufferReaderConsume(trak->stsz_data.reader, pass);
    TSIOBufferReaderFree(readerp);

    return 0;
}

int
Mp4Meta::mp4_update_co64_atom(Mp4Trak *trak)
{
    int64_t             atom_size, avail, pass;
    TSIOBufferReader    readerp;

    if (trak->co64_data.buffer == NULL)
        return -1;

    if (trak->start_chunk > trak->chunks)
        return -1;

    readerp = trak->co64_data.reader;
    avail = TSIOBufferReaderAvail(readerp);

    pass = trak->start_chunk * sizeof(uint64_t);
    atom_size = sizeof(mp4_co64_atom) + avail - pass;
    trak->size += atom_size;

    TSIOBufferReaderConsume(readerp, pass);
    trak->start_offset = mp4_reader_get_64value(readerp, 0);
    trak->start_offset += trak->chunk_samples_size;
    mp4_reader_set_64value(readerp, 0, trak->start_offset);

    mp4_reader_set_32value(trak->co64_atom.reader, offsetof(mp4_co64_atom, size),
                           atom_size);
    mp4_reader_set_32value(trak->co64_atom.reader, offsetof(mp4_co64_atom, entries),
                           trak->chunks - trak->start_chunk);

    return 0;
}

int
Mp4Meta::mp4_update_stco_atom(Mp4Trak *trak)
{
    int64_t             atom_size, avail;
    uint32_t            pass;
    TSIOBufferReader    readerp;

    if (trak->stco_data.buffer == NULL)
        return -1;

    if (trak->start_chunk > trak->chunks)
        return -1;

    readerp = trak->stco_data.reader;
    avail = TSIOBufferReaderAvail(readerp);

    pass = trak->start_chunk * sizeof(uint32_t);
    atom_size = sizeof(mp4_stco_atom) + avail - pass;
    trak->size += atom_size;

    TSIOBufferReaderConsume(readerp, pass);

    trak->start_offset = mp4_reader_get_32value(readerp, 0);
    trak->start_offset += trak->chunk_samples_size;
    mp4_reader_set_32value(readerp, 0, trak->start_offset);

    mp4_reader_set_32value(trak->stco_atom.reader, offsetof(mp4_stco_atom, size),
                           atom_size);
    mp4_reader_set_32value(trak->stco_atom.reader, offsetof(mp4_stco_atom, entries),
                           trak->chunks - trak->start_chunk);

    return 0;
}

int
Mp4Meta::mp4_update_stbl_atom(Mp4Trak *trak)
{
    trak->size += sizeof(mp4_atom_header);
    mp4_reader_set_32value(trak->stbl_atom.reader, 0, trak->size);

    return 0;
}

int
Mp4Meta::mp4_update_minf_atom(Mp4Trak *trak)
{
    trak->size += sizeof(mp4_atom_header) +
                  trak->vmhd_size +
                  trak->smhd_size +
                  trak->dinf_size;

    mp4_reader_set_32value(trak->minf_atom.reader, 0, trak->size);

    return 0;
}

int
Mp4Meta::mp4_update_mdia_atom(Mp4Trak *trak)
{
    trak->size += sizeof(mp4_atom_header);
    mp4_reader_set_32value(trak->mdia_atom.reader, 0, trak->size);

    return 0;
}

int
Mp4Meta::mp4_update_trak_atom(Mp4Trak *trak)
{
    trak->size += sizeof(mp4_atom_header);
    mp4_reader_set_32value(trak->trak_atom.reader, 0, trak->size);

    return 0;
}

int
Mp4Meta::mp4_adjust_co64_atom(Mp4Trak *trak, off_t adjustment)
{
    int64_t             pos, avail;
    TSIOBufferReader    readerp;

    readerp = TSIOBufferReaderClone(trak->co64_data.reader);
    avail = TSIOBufferReaderAvail(readerp);

    for (pos = 0; pos < avail; pos += sizeof(uint64_t)) {
        offset = mp4_reader_get_64value(readerp, 0);
        offset += adjustment;
        mp4_reader_set_64value(readerp, 0, offset);
        TSIOBufferReaderConsume(readerp, sizeof(uint64_t));
    }

    TSIOBufferReaderFree(readerp);

    return 0;
}

int
Mp4Meta::mp4_adjust_stco_atom(Mp4Trak *trak, int32_t adjustment)
{
    int64_t             pos, avail, offset;
    TSIOBufferReader    readerp;

    readerp = TSIOBufferReaderClone(trak->stco_data.reader);
    avail = TSIOBufferReaderAvail(readerp);

    for (pos = 0; pos < avail; pos += sizeof(uint32_t)) {
        offset = mp4_reader_get_32value(readerp, 0);
        offset += adjustment;
        mp4_reader_set_32value(readerp, 0, offset);
        TSIOBufferReaderConsume(readerp, sizeof(uint32_t));
    }

    TSIOBufferReaderFree(readerp);

    return 0;
}

int64_t
Mp4Meta::mp4_update_mdat_atom(int64_t start_offset)
{
    int64_t     atom_data_size;
    int64_t     atom_size;
    int64_t     atom_header_size;
    u_char      *atom_header;

    atom_data_size = this->cl - start_offset;
    this->start_pos = start_offset;

    atom_header = mdat_atom_header;

    if (atom_data_size > 0xffffffff) {
        atom_size = 1;
        atom_header_size = sizeof(mp4_atom_header64);
        mp4_set_64value(atom_header + sizeof(mp4_atom_header),
                        sizeof(mp4_atom_header64) + atom_data_size);

    } else {
        atom_size = sizeof(mp4_atom_header) + atom_data_size;
        atom_header_size = sizeof(mp4_atom_header);
    }

    this->content_length += atom_header_size + atom_data_size;

    mp4_set_32value(atom_header, atom_size);
    mp4_set_atom_name(atom_header, 'm', 'd', 'a', 't');

    mdat_atom.buffer = TSIOBufferSizedCreate(TS_IOBUFFER_SIZE_INDEX_128);
    mdat_atom.reader = TSIOBufferReaderAlloc(mdat_atom.buffer);

    TSIOBufferWrite(mdat_atom.buffer, atom_header, atom_header_size);

    return atom_header_size;
}


uint32_t
Mp4Meta::mp4_find_key_frame(uint32_t start_sample, Mp4Trak *trak)
{
    uint32_t            i;
    uint32_t            sample, prev_sample, entries;
    TSIOBufferReader    readerp;

    if (trak->stss_data.buffer == NULL)
        return start_sample;

    prev_sample = 1;
    entries = trak->sync_samples_entries;

    readerp = TSIOBufferReaderClone(trak->stss_data.reader);

    for (i = 0; i < entries; i++) {
        sample = (uint32_t)mp4_reader_get_32value(readerp, 0);  // 获得帧号码

        if (sample > start_sample) {                            // 因为start_sample是从0开始, 所以不能包含等于条件
            goto found;
        }

        prev_sample = sample;
        TSIOBufferReaderConsume(readerp, sizeof(uint32_t));
    }

    TSIOBufferReaderFree(readerp);
    return start_sample;

found:

    TSIOBufferReaderFree(readerp);
    return prev_sample;
}


void
mp4_reader_set_32value(TSIOBufferReader readerp, int64_t offset, uint32_t n)
{
    int                 pos;
    int64_t             avail, left;
    TSIOBufferBlock     blk;
    const char          *start;
    u_char              *ptr;

    pos = 0;
    blk = TSIOBufferReaderStart(readerp);

    while (blk) {

        start = TSIOBufferBlockReadStart(blk, readerp, &avail);

        if (avail <= offset) {
            offset -= avail;

        } else {

            left = avail - offset;
            ptr = (u_char*)(const_cast<char*> (start) + offset);

            while (pos < 4 && left > 0) {
                 *ptr++ = (u_char) ((n) >> ((3 - pos) * 8));
                 pos++;
                 left--;
            }

            if (pos >= 4)
                return;

            offset = 0;
        }

        blk = TSIOBufferBlockNext(blk);
    }
}

void
mp4_reader_set_64value(TSIOBufferReader readerp, int64_t offset, uint64_t n)
{
    int                 pos;
    int64_t             avail, left;
    TSIOBufferBlock     blk;
    const char          *start;
    u_char              *ptr;

    pos = 0;
    blk = TSIOBufferReaderStart(readerp);

    while (blk) {

        start = TSIOBufferBlockReadStart(blk, readerp, &avail);

        if (avail <= offset) {
            offset -= avail;

        } else {

            left = avail - offset;
            ptr = (u_char*)(const_cast<char*> (start) + offset);

            while (pos < 8 && left > 0) {
                 *ptr++ = (u_char) ((n) >> ((7 - pos) * 8));
                 pos++;
                 left--;
            }

            if (pos >= 4)
                return;

            offset = 0;
        }

        blk = TSIOBufferBlockNext(blk);
    }
}

uint32_t
mp4_reader_get_32value(TSIOBufferReader readerp, int64_t offset)
{
    int                 pos;
    int64_t             avail, left;
    TSIOBufferBlock     blk;
    const char          *start;
    const u_char        *ptr;
    u_char              res[4];

    pos = 0;
    blk = TSIOBufferReaderStart(readerp);

    while (blk) {

        start = TSIOBufferBlockReadStart(blk, readerp, &avail);

        if (avail <= offset) {
            offset -= avail;

        } else {

            left = avail - offset;
            ptr = (u_char*)(start + offset);

            while (pos < 4 && left > 0) {
                res[3-pos] = *ptr++;
                pos++;
                left--;
            }

            if (pos >= 4) {
                return *(uint32_t*)res;
            }

            offset = 0;
        }

        blk = TSIOBufferBlockNext(blk);
    }

    return -1;
}

uint64_t
mp4_reader_get_64value(TSIOBufferReader readerp, int64_t offset)
{
    int                 pos;
    int64_t             avail, left;
    TSIOBufferBlock     blk;
    const char          *start;
    u_char              *ptr;
    u_char              res[8];

    pos = 0;
    blk = TSIOBufferReaderStart(readerp);

    while (blk) {

        start = TSIOBufferBlockReadStart(blk, readerp, &avail);

        if (avail <= offset) {
            offset -= avail;

        } else {

            left = avail - offset;
            ptr = (u_char*)(start + offset);

            while (pos < 8 && left > 0) {
                res[7-pos] = *ptr++;
                pos++;
                left--;
            }

            if (pos >= 8) {
                return *(uint64_t*)res;
            }

            offset = 0;
        }

        blk = TSIOBufferBlockNext(blk);
    }

    return -1;
}
