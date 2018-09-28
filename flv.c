/*
  * A H264/AAC TO FLV implementation
  *
  * Copyright (C) 2014 rainfly123 <xiechc@gmail.com>
  *
  * This program is free software; you can redistribute it and/or modify
  * it under the terms of the GNU General Public License as published by
  * the Free Software Foundation; either version 2 of the License, or
  * (at your option) any later version.
  *
  * This program is distributed in the hope that it will be useful,
  * but WITHOUT ANY WARRANTY; without even the implied warranty of
  * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  * GNU General Public License for more details.
  *
  * You should have received a copy of the GNU General Public License
  * along with this program; if not, write to the Free Software
  * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
  *
  */
#include "flv.h"

#define FRAME_PER_SEC 10

static uint32_t find_start_code(uint8_t *buf, uint32_t zeros_in_startcode)   
{   
  uint32_t info;   
  uint32_t i;   
   
  info = 1;   
  if ((info = (buf[zeros_in_startcode] != 1)? 0: 1) == 0)   
      return 0;   
       
  for (i = 0; i < zeros_in_startcode; i++)   
    if (buf[i] != 0)   
    { 
        info = 0;
        break;
    };   
     
  return info;   
}   

uint8_t * get_nal(uint32_t *len, uint8_t **offset, uint8_t *start, uint32_t total)
{
    uint32_t info;
    uint8_t *q ;
    uint8_t *p  =  *offset;
    *len = 0;

    while(1) {
        info =  find_start_code(p, 3);
        if (info == 1)
            break;
        p++;
        if ((p - start) >= total - 3) // prevent overflow
            return NULL;
    }
    q = p + 4;
    p = q;
    while(1) {
        info =  find_start_code(p, 3);
        if (info == 1)
            break;
        p++;
        if ((p - start) >= total)
            return NULL;
    }
    
    *len = (p - q);
    *offset = p;
    return q;
}

#define AAC_ADTS_HEADER_SIZE 7
#define FLV_TAG_HEAD_LEN 11
#define FLV_PRE_TAG_LEN 4

typedef struct {
    uint8_t audio_object_type;
    uint8_t sample_frequency_index;
    uint8_t channel_configuration;
}AudioSpecificConfig;

AudioSpecificConfig gen_config(uint8_t *frame)
{
    AudioSpecificConfig config = {0, 0, 0};

    if (frame == NULL) {
        return config;
    }
    config.audio_object_type = (frame[2] & 0xc0) >> 6;
    config.sample_frequency_index =  (frame[2] & 0x3c) >> 2;
    config.channel_configuration = (frame[3] & 0xc0) >> 6;
    return config;
}

uint8_t gen_audio_tag_header(AudioSpecificConfig config)
{
     uint8_t soundType = config.channel_configuration - 1; //0 mono, 1 stero
     uint8_t soundRate = 0;
     uint8_t val = 0;


     switch (config.sample_frequency_index) {
         case 10: { //11.025k
             soundRate = 1;
             break;
         }
         case 7: { //22k
             soundRate = 2;
             break;
         }
         case 4: { //44k
             soundRate = 3;
             break;
         }
         default:
         { 
             return val;
         }
    }
    val = 0xA0 | (soundRate << 2) | 0x02 | soundType;
    return val;
}

uint8_t *get_adts(uint32_t *len, uint8_t **offset, uint8_t *start, uint32_t total)
{
    uint8_t *p  =  *offset;
    uint32_t frame_len_1;
    uint32_t frame_len_2;
    uint32_t frame_len_3;
    uint32_t frame_length;
   
    if (total < AAC_ADTS_HEADER_SIZE) {
        return NULL;
    }
    if ((p - start) >= total) {
        return NULL;
    }
    
    if (p[0] != 0xff) {
        return NULL;
    }
    if ((p[1] & 0xf0) != 0xf0) {
        return NULL;
    }
    frame_len_1 = p[3] & 0x03;
    frame_len_2 = p[4];
    frame_len_3 = (p[5] & 0xe0) >> 5;
    frame_length = (frame_len_1 << 11) | (frame_len_2 << 3) | frame_len_3;
    *offset = p + frame_length;
    *len = frame_length;
    return p;
}

/*
 * Mux h264 byte stream into flv container.
 */
void write_flv(char* data, int len, const char* filename)
{
    uint8_t *buf = data;
    uint8_t *buf_offset = buf;
    uint32_t total = len;
    int flv_file = open(filename, O_WRONLY|O_CREAT, 0666);
    // FLV Header for video only (byte 5 = 0x01) 
    uint8_t flv_header[13] = {0x46, 0x4c, 0x56, 0x01, 0x01, 0x00, 0x00, 0x00, 0x09, \
                             0x00, 0x00,0x00,0x00};
    write(flv_file, flv_header, sizeof(flv_header));
    uint32_t nal_len;
    uint32_t nal_len_n;
    uint8_t *nal; 
    uint8_t *nal_n;
    uint8_t *output ; 
    uint32_t offset = 0;
    uint32_t body_len;
    uint32_t ts = 0;
    uint32_t output_len;

    while(1)    {
    //Video OUTPUT 
    offset = 0;
    nal = get_nal(&nal_len, &buf_offset, buf, total);
    if (nal == NULL) break;
    if (nal[0] == 0x67)  {
        nal_n  = get_nal(&nal_len_n, &buf_offset, buf, total); //get pps
        body_len = nal_len + nal_len_n + 16;
        output_len = body_len + FLV_TAG_HEAD_LEN + FLV_PRE_TAG_LEN;
        output = malloc(output_len);
        // flv tag header
        output[offset++] = 0x09; //tagtype video
        output[offset++] = (uint8_t)(body_len >> 16); //data len
        output[offset++] = (uint8_t)(body_len >> 8); //data len
        output[offset++] = (uint8_t)(body_len); //data len
        output[offset++] = (uint8_t)(ts >> 16); //time stamp
        output[offset++] = (uint8_t)(ts >> 8); //time stamp
        output[offset++] = (uint8_t)(ts); //time stamp
        output[offset++] = (uint8_t)(ts >> 24); //time stamp
        output[offset++] = 0x00; //stream id 0
        output[offset++] = 0x00; //stream id 0
        output[offset++] = 0x00; //stream id 0

        //flv VideoTagHeader
        output[offset++] = 0x17; //key frame, AVC
        output[offset++] = 0x00; //avc sequence header
        output[offset++] = 0x00; //composit time ??????????
        output[offset++] = 0x00; // composit time
        output[offset++] = 0x00; //composit time

        //flv VideoTagBody --AVCDecoderCOnfigurationRecord
        output[offset++] = 0x01; //configurationversion
        output[offset++] = nal[1]; //avcprofileindication
        output[offset++] = nal[2]; //profilecompatibilty
        output[offset++] = nal[3]; //avclevelindication
        output[offset++] = 0xff; //reserved + lengthsizeminusone
        output[offset++] = 0xe1; //numofsequenceset
        output[offset++] = (uint8_t)(nal_len >> 8); //sequence parameter set length high 8 bits
        output[offset++] = (uint8_t)(nal_len); //sequence parameter set  length low 8 bits
        memcpy(output + offset, nal, nal_len); //H264 sequence parameter set
        offset += nal_len;
        output[offset++] = 0x01; //numofpictureset
        output[offset++] = (uint8_t)(nal_len_n >> 8); //picture parameter set length high 8 bits
        output[offset++] = (uint8_t)(nal_len_n); //picture parameter set length low 8 bits
        memcpy(output + offset, nal_n, nal_len_n); //H264 picture parameter set

        // flv test 
        offset += nal_len_n;
        uint32_t fff = body_len + FLV_TAG_HEAD_LEN;
        output[offset++] = (uint8_t)(fff >> 24); //data len
        output[offset++] = (uint8_t)(fff >> 16); //data len
        output[offset++] = (uint8_t)(fff >> 8); //data len
        output[offset++] = (uint8_t)(fff); //data len
        write(flv_file, output, output_len);
       //RTMP Send out
        free(output);
        continue;
    }
    if (nal[0] == 0x06)
    {    //do nothin
    }

    if (nal[0] == 0x65)
    {
        ts += 1000 / FRAME_PER_SEC;
        body_len = nal_len + 5 + 4; //flv VideoTagHeader +  NALU length
        output_len = body_len + FLV_TAG_HEAD_LEN + FLV_PRE_TAG_LEN;
        output = malloc(output_len);
        // flv tag header
        output[offset++] = 0x09; //tagtype video
        output[offset++] = (uint8_t)(body_len >> 16); //data len
        output[offset++] = (uint8_t)(body_len >> 8); //data len
        output[offset++] = (uint8_t)(body_len); //data len
        output[offset++] = (uint8_t)(ts >> 16); //time stamp
        output[offset++] = (uint8_t)(ts >> 8); //time stamp
        output[offset++] = (uint8_t)(ts); //time stamp
        output[offset++] = (uint8_t)(ts >> 24); //time stamp
        output[offset++] = 0x00; //stream id 0
        output[offset++] = 0x00; //stream id 0
        output[offset++] = 0x00; //stream id 0

        //flv VideoTagHeader
        output[offset++] = 0x17; //key frame, AVC
        output[offset++] = 0x01; //avc NALU unit
        output[offset++] = 0x00; //composit time ??????????
        output[offset++] = 0x00; // composit time
        output[offset++] = 0x00; //composit time

        output[offset++] = (uint8_t)(nal_len >> 24); //nal length 
        output[offset++] = (uint8_t)(nal_len >> 16); //nal length 
        output[offset++] = (uint8_t)(nal_len >> 8); //nal length 
        output[offset++] = (uint8_t)(nal_len); //nal length 
        memcpy(output + offset, nal, nal_len);

        //no need set pre_tag_size ,RTMP NO NEED
        offset += nal_len;
        uint32_t fff = body_len + FLV_TAG_HEAD_LEN;
        output[offset++] = (uint8_t)(fff >> 24); //data len
        output[offset++] = (uint8_t)(fff >> 16); //data len
        output[offset++] = (uint8_t)(fff >> 8); //data len
        output[offset++] = (uint8_t)(fff); //data len
        write(flv_file, output, output_len);
       //RTMP Send out
        free(output);
        continue;
     }

    if (nal[0] == 0x61)
    {
        ts += 1000 / FRAME_PER_SEC;
        body_len = nal_len + 5 + 4; //flv VideoTagHeader +  NALU length
        output_len = body_len + FLV_TAG_HEAD_LEN + FLV_PRE_TAG_LEN;
        output = malloc(output_len);
        // flv tag header
        output[offset++] = 0x09; //tagtype video
        output[offset++] = (uint8_t)(body_len >> 16); //data len
        output[offset++] = (uint8_t)(body_len >> 8); //data len
        output[offset++] = (uint8_t)(body_len); //data len
        output[offset++] = (uint8_t)(ts >> 16); //time stamp
        output[offset++] = (uint8_t)(ts >> 8); //time stamp
        output[offset++] = (uint8_t)(ts); //time stamp
        output[offset++] = (uint8_t)(ts >> 24); //time stamp
        output[offset++] = 0x00; //stream id 0
        output[offset++] = 0x00; //stream id 0
        output[offset++] = 0x00; //stream id 0

        //flv VideoTagHeader
        output[offset++] = 0x27; //key frame, AVC
        output[offset++] = 0x01; //avc NALU unit
        output[offset++] = 0x00; //composit time ??????????
        output[offset++] = 0x00; // composit time
        output[offset++] = 0x00; //composit time

        output[offset++] = (uint8_t)(nal_len >> 24); //nal length 
        output[offset++] = (uint8_t)(nal_len >> 16); //nal length 
        output[offset++] = (uint8_t)(nal_len >> 8); //nal length 
        output[offset++] = (uint8_t)(nal_len); //nal length 
        memcpy(output + offset, nal, nal_len);

        //no need set pre_tag_size ,RTMP NO NEED
        offset += nal_len;
        uint32_t fff = body_len + FLV_TAG_HEAD_LEN;
        output[offset++] = (uint8_t)(fff >> 24); //data len
        output[offset++] = (uint8_t)(fff >> 16); //data len
        output[offset++] = (uint8_t)(fff >> 8); //data len
        output[offset++] = (uint8_t)(fff); //data len
        write(flv_file, output, output_len);

       //RTMP Send out
        free(output);
        continue;
     }
   }
}
