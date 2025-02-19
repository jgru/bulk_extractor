/**
 * Plugin: scan_ntfsmft
 * Purpose: Find all MFT file record into one file
 * Reference: http://www.digital-evidence.org/fsfa/
 * Teru Yamazaki(@4n6ist) - https://github.com/4n6ist/bulk_extractor-rec
 **/

#include <iostream>
#include <fstream>
#include <string>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <vector>
#include <cerrno>

#include "config.h"
#include "be20_api/scanner_params.h"

#include "utf8.h"


#define SECTOR_SIZE 512
#define CLUSTER_SIZE 4096
#define MFT_RECORD_SIZE 1024
#define FEATURE_FILE_NAME "ntfsmft_carved"


// check MFT Record Signature
// return: 1 - valid MFT record, 2 - corrupt MFT record, 0 - not MFT record
int8_t check_mftrecord_signature(size_t offset, const sbuf_t &sbuf) {
    int16_t fixup_offset;
    int16_t fixup_count;
    int16_t fixup_value;

    if (sbuf[offset] == 0x46 && sbuf[offset + 1] == 0x49 &&
        sbuf[offset + 2] == 0x4c  && sbuf[offset + 3] == 0x45) {

        fixup_offset = sbuf.get16i(offset + 4);
        if (fixup_offset <= 0 || fixup_offset >= SECTOR_SIZE)
            return 0;
        fixup_count = sbuf.get16i(offset + 6);
        if (fixup_count <= 0 || fixup_count >= SECTOR_SIZE)
            return 0;

        fixup_value = sbuf.get16i(offset + fixup_offset);

        for(int i=1;i<fixup_count;i++){
            if (fixup_value != sbuf.get16i(offset + (SECTOR_SIZE * i) - 2))
                return 2;
        }
        return 1;
    } else {
        return 0;
    }
}

extern "C"

void scan_ntfsmft(scanner_params &sp)
{
    sp.check_version();
    if(sp.phase==scanner_params::PHASE_INIT){
        sp.info->set_name("ntfsmft");
        sp.info->author          = "Teru Yamazaki";
        sp.info->description     = "Scans for NTFS MFT record";
        sp.info->scanner_version = "1.0";
        struct feature_recorder_def::flags_t carve_flag;
        carve_flag.carve = true;
        sp.info->feature_defs.push_back( feature_recorder_def(FEATURE_FILE_NAME, carve_flag));
        sp.info->scanner_flags.scanner_wants_filesystems = true;
        return;
    }
    if(sp.phase==scanner_params::PHASE_SCAN){
        const sbuf_t &sbuf = (*sp.sbuf);
        feature_recorder &ntfsmft_recorder = sp.named_feature_recorder(FEATURE_FILE_NAME);

        // search for NTFS MFT record in the sbuf
        size_t offset = 0;
        size_t stop = sbuf.pagesize;
        size_t total_record_size=0;
        int8_t result_type;

        while (offset < stop) {

            result_type = check_mftrecord_signature(offset, sbuf);
            total_record_size = MFT_RECORD_SIZE;

            if (result_type == 1) {

                // found one valid record then also checks following valid records and writes all at once
                while (true) {
                    if (offset+total_record_size >= stop)
                        break;

                    result_type = check_mftrecord_signature(offset+total_record_size, sbuf);

                    if (result_type == 1)
                        total_record_size += MFT_RECORD_SIZE;
                    else
                        break;
                }
                ntfsmft_recorder.carve(sbuf_t(sbuf,offset,total_record_size),".mft");
            }
            else if (result_type == 2) {
                ntfsmft_recorder.carve(sbuf_t(sbuf,offset,total_record_size),".mft_corrputed");
            }
            else { // result_type == 0 - not MFT record
            }
            offset += total_record_size;
        }
    }
}
