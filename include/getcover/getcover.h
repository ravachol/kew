/*
 * getcover: Extract picture data from audio files
 *
 * Copyright (c) 2015 Yasuyuki Suzuki
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
 * RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF
 * CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * compile with: gcc -o getcover getcover.c
 *
 * Usage: %s [-v] [-o] [-f basename] path [path [path]...]
 *  -v: verbose mode
 *  -o: override mode. override image file even if it exists
 *  -f basename: specifiy name of image file without suffix
 *  path: path of the directory which contains FLAC or mp4 files
 *
 * 
 */

#ifndef GETCOVER_H
#define GETCOVER_H

/*============================================================================
 Includes
 ============================================================================*/

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

/*============================================================================
 Global variables, type definitions and prototypes
 ============================================================================*/

#define MAXPATHLENGTH 1000
#define DONE 1
#define NOTYET 0
int verbose = 0;
int override = 0;
int nbasename = 0;
static char jpegFile[MAXPATHLENGTH] = "cover.jpg";
static char pngFile[MAXPATHLENGTH] = "cover.png";

static void extract_cover(const char *arg, char *outputFilePath);
static int get_FLAC_cover(FILE *fp, const char *dirpath);
static int get_m4a_cover(FILE *fp, const char *dirpath);
static int get_mp3_cover(const char *filepath, const char *outputfilepath);
static int extractID3v2_cover(const char* filePath, const char* outputFilePath);
static int extractMP3Cover(const char* inputFilePath, const char* outputFilePath);
extern long getFileSize(FILE* file);

/*============================================================================
 Function for extracting the album cover to the designated directory
 ============================================================================*/

static void extract_cover(const char *arg, char *outputFilePath)
{
    DIR *dirp;
    struct dirent *dp;
    FILE *fp;
    char str[10] = "";
    char filepath[MAXPATHLENGTH];
    unsigned int offset;    
    size_t size;    

    if ((dirp = opendir(arg)) == NULL) {
        fprintf(stderr, "Could not open dir:%s\n", arg);
        perror("opendir()");
        return;
    }
    
    do {
        if ((dp = readdir(dirp)) != NULL)
            switch(dp->d_type) {
                case DT_DIR: /* directory */
                    if(verbose) (void) printf("DIR: %s\n", dp->d_name);
                    break;
                case DT_REG: /* regular file */
                    strcpy(filepath, arg);
                    //strcat(filepath, "/");
                    strcat(filepath, dp->d_name);
                    if((fp = fopen(filepath, "r")) != NULL) {
                        size = fread(str, 1, 4, fp);
                        if(memcmp(str,"fLaC", 4)==0) { /* If FLAC, get cover */
                            if(verbose) (void) printf("FLAC FILE: %s\n", dp->d_name);
                            if(get_FLAC_cover(fp, arg) == DONE)
                                return;
                        }
                        else if(memcmp(str,"RIFF", 4)==0) {
                            if(verbose) (void) printf("WAV FILE: %s\n", dp->d_name);
                        }
                        else if(memcmp(str,"ID3\3", 4)==0) {
                            if(verbose) (void) printf("mp3 FILE: %s\n", dp->d_name);
                          
                            strcpy(outputFilePath, arg);
                            strcat(outputFilePath, "cover.jpg");

                            if(extractMP3Cover(filepath,outputFilePath) == DONE)
                            {
                                return;
                            }
                        }
                        else {
                            offset = ((str[0]<<24)|(str[1]<<16)|(str[2]<<8)|str[3]);
                            size = fread(str, 1, 4, fp);                            
                            if(memcmp(str,"ftyp", 4)==0) {
                                if(verbose) (void) printf("MP4 FILE: %s\n", dp->d_name);
                                if(get_m4a_cover(fp, arg) == DONE)
                                    return;
                            }
                            else {
                                if(verbose) (void) printf("Other FILE: %s\n", dp->d_name);
                            }
                        }
                        fclose(fp);
                        break;
                    default:
                        if(verbose) (void) printf("Special file: %s\n", dp->d_name);
                    } else {
                        //perror("lookup()::fopen()");
                        return;
                    }
            }
    } while (dp != NULL);
    
    (void) closedir(dirp);
    return;    
}

/*============================================================================
 Functions for extracting JPEG/PNG data from a MP3 file
 ============================================================================*/

// Check if the file contains ID3v2.3 or ID3v2.4 tag
int checkIfV2(const char* filePath)
{
    FILE* file = fopen(filePath, "rb");
    if (file == NULL) {
        printf("Failed to open the file.\n");
        return 0;  // Assuming 0 means false
    }

    char formatId[4];
    if (fread(formatId, sizeof(char), 3, file) != 3) {
        fclose(file);
        return 0;  // Failed to read format ID
    }
    formatId[3] = '\0';

    unsigned short version;
    if (fread(&version, sizeof(unsigned short), 1, file) != 1) {
        fclose(file);
        return 0;  // Failed to read version
    }

    unsigned char headerFlag;
    if (fread(&headerFlag, sizeof(unsigned char), 1, file) != 1) {
        fclose(file);
        return 0;  // Failed to read header flag
    }

    if (verbose) printf("Format ID: %s\n", formatId);
    if (verbose) printf("Version: %04X\n", version);
    if (verbose) printf("Header Flag: %02X\n", headerFlag);

    // Check format
    int result = (strcmp(formatId, "ID3") == 0 && (version == 0x0300U || version == 0x0400U));

    // Check if there is no extended header
    result = result && ((headerFlag & 0x0040U) == 0);

    fclose(file);
    return result;
}
/*
// Check if the file contains ID3v2.2 tag
int checkIfV22(const char* filePath)
{
    FILE* file = fopen(filePath, "rb");
    if (file == NULL) {
        if (verbose) printf("Failed to open the file.\n");
        return 0;  // Assuming 0 means false
    }

    char formatId[4];
    if (fread(formatId, sizeof(char), 3, file) != 3) {
        fclose(file);
        return 0;  // Failed to read format ID
    }
    formatId[3] = '\0';

    unsigned short version;
    if (fread(&version, sizeof(unsigned short), 1, file) != 1) {
        fclose(file);
        return 0;  // Failed to read version
    }

    // Check format
    int result = (strcmp(formatId, "ID3") == 0 && version == 0x0200U);

    fclose(file);
    return result;
}*/

long getFileSize(FILE* file) {
    if (file == NULL) {
        return -1;  // Failed to open the file
    }

    long currentPosition = ftell(file);  // Store the current position

    fseek(file, 0, SEEK_END);  // Move the file pointer to the end of the file
    long fileSize = ftell(file);  // Get the file size

    fseek(file, currentPosition, SEEK_SET);  // Restore the file pointer to the original position

    return fileSize;
}

/*   id3v2.4 Attached picture

   This frame contains a picture directly related to the audio file.
   Image format is the MIME type and subtype [MIME] for the image. In
   the event that the MIME media type name is omitted, "image/" will be
   implied. The "image/png" [PNG] or "image/jpeg" [JFIF] picture format
   should be used when interoperability is wanted. Description is a
   short description of the picture, represented as a terminated
   text string. There may be several pictures attached to one file, each
   in their individual "APIC" frame, but only one with the same content
   descriptor. There may only be one picture with the picture type
   declared as picture type $01 and $02 respectively. There is the
   possibility to put only a link to the image file by using the 'MIME
   type' "-->" and having a complete URL [URL] instead of picture data.
   The use of linked files should however be used sparingly since there
   is the risk of separation of files.

     <Header for 'Attached picture', ID: "APIC">
     Text encoding      $xx
     MIME type          <text string> $00
     Picture type       $xx
     Description        <text string according to encoding> $00 (00)
     Picture data       <binary data>


   Picture type:  $00  Other
                  $01  32x32 pixels 'file icon' (PNG only)
                  $02  Other file icon
                  $03  Cover (front)
                  $04  Cover (back)
                  $05  Leaflet page
                  $06  Media (e.g. label side of CD)
                  $07  Lead artist/lead performer/soloist
                  $08  Artist/performer
                  $09  Conductor
                  $0A  Band/Orchestra
                  $0B  Composer
                  $0C  Lyricist/text writer
                  $0D  Recording Location
                  $0E  During recording
                  $0F  During performance
                  $10  Movie/video screen capture
                  $11  A bright coloured fish
                  $12  Illustration
                  $13  Band/artist logotype
                  $14  Publisher/Studio logotype
*/

#include <stdlib.h> // For size_t
#include <stdio.h>  // For FILE, fopen, fread, fwrite, fseek, ftell, fclose

int extractMP3Cover(const char* inputFilePath, const char* outputFilePath) 
{
    char command[256];
    snprintf(command, sizeof(command), "ffmpeg -y -i \"%s\" -an -vcodec copy \"%s\"", inputFilePath, outputFilePath);

    FILE* pipe = popen(command, "r");
    if (pipe == NULL) {
        printf("Failed to run FFmpeg command.\n");
        return -1;
    }
    pclose(pipe);

    return 0;
}


int extractID3v2_cover(const char* filePath, const char* outputFilePath) {
    const size_t bufferSize = 4096;

    FILE* file = fopen(filePath, "rb");
    if (file == NULL) {
        if (verbose) printf("Failed to open the file.\n");
        return -1;
    }

    unsigned char header[2];
    long long startPosition = -1;  // Store the position of the header
    while (fread(header, sizeof(unsigned char), 2, file) == 2) {
        if (header[0] == 0xFF && (header[1] == 0xD8 || header[1] == 0xE0)) {
            // Found JPEG header (0xFFD8) or APP0 marker (0xFFE0)
            startPosition = ftell(file) - 2;  // Store the position of the header
            break;
        } else if (header[0] == 0x89 && header[1] == 0x50) {
            // Found PNG header (0x8950)
            startPosition = ftell(file) - 2;  // Store the position of the header
            break;
        }
    }

    if (startPosition == -1) {
        if (verbose) printf("Cover image header not found.\n");
        fclose(file);
        return -1;
    }

    fseek(file, startPosition, SEEK_SET);  // Set the file position to the start of the header

    FILE* outputFile = fopen(outputFilePath, "wb");
    if (outputFile == NULL) {
        if (verbose) printf("Failed to create output file.\n");
        fclose(file);
        return -1;
    }

    unsigned char buffer[bufferSize];
    size_t bytesRead;

    // Write the image data to the output file
    while ((bytesRead = fread(buffer, sizeof(unsigned char), bufferSize, file)) > 0) {
        fwrite(buffer, sizeof(unsigned char), bytesRead, outputFile);
    }

    // Search for the JPEG footer marker (0xFFD9) to ensure the entire image data is written
    fseek(file, startPosition, SEEK_SET);  // Set the file position to the start of the header
    while (fread(header, sizeof(unsigned char), 2, file) == 2) {
        if (header[0] == 0xFF && header[1] == 0xD9) {
            if (verbose) printf("Found JPEG footer!\n");
            fclose(file);
            fclose(outputFile);
            if (verbose) printf("Cover image saved to: %s\n", outputFilePath);
            return 0;
        }
        fwrite(header, sizeof(unsigned char), 2, outputFile);
    }

    fclose(outputFile);
    fclose(file);

    return -1;
}

/*
int extractID3v2_cover(const char* filePath, const char* outputFilePath) {
    const size_t bufferSize = 4096;
    FILE* file = fopen(filePath, "rb");
    if (file == NULL) {
        if (verbose) printf("Failed to open the file.\n");
        return -1;
    }

    // Search for JPEG or PNG header
    unsigned char header[2];
    while (fread(header, sizeof(unsigned char), 2, file) == 2) {
        if (header[0] == 0xFF && (header[1] == 0xD8 || header[1] == 0xE0)) {
            // Found JPEG header (0xFFD8) or APP0 marker (0xFFE0)
            break;
        } else if (header[0] == 0x89 && header[1] == 0x50) {
            // Found PNG header (0x8950)
            break;
        }
    }

    // Check if header was found
    if (feof(file)) {
        if (verbose) printf("Cover image header not found.\n");
        fclose(file);
        return -1;
    }

    // Move file position back to the start of the header
    fseek(file, -2, SEEK_CUR);

    // Read and write the image data to the output file
    FILE* outputFile = fopen(outputFilePath, "wb");
    if (outputFile == NULL) {
        if (verbose) printf("Failed to create output file.\n");
        fclose(file);
        return -1;
    }

    unsigned char buffer[bufferSize];
    size_t bytesRead;

    while ((bytesRead = fread(buffer, sizeof(unsigned char), sizeof(buffer), file)) > 0) {
        fwrite(buffer, sizeof(unsigned char), bytesRead, outputFile);

        // Search for JPEG markers
        for (size_t i = 0; i < bytesRead - 1; i++) {
            if (buffer[i] == 0xFF && buffer[i + 1] == 0xD9) {
                if (verbose) printf("Found JPEG footer!\n");
                fclose(file);
                fclose(outputFile);
                if (verbose) printf("Cover image saved to: %s\n", outputFilePath);
                return 0;  // Reached the end of the JPEG image data
            }
        }
    }

   fclose(outputFile);
    fclose(file);


    return -1;
}
*/
int get_mp3_cover(const char* filePath, const char *outputfilepath) 
{
   /* if (checkIfV2(filePath))
        printf("v2");
    else if (checkIfV22(filePath))
        printf("v22");
    else
        printf("id3 version not found");
*/

    int result = extractID3v2_cover(filePath, outputfilepath);

    if (result == 0) 
        return DONE; // FIXME: refactor this so done is not used anymore
    else
        return -1;
}

/*============================================================================
 Function for extracting JPEG/PNG data from a FLAC file
 ============================================================================*/

static int get_FLAC_cover(FILE *fp, const char *outputFilepath)
{
    unsigned char block_header;
    unsigned char length[3];
    unsigned int ilength;
    unsigned char buf[4];
    unsigned int picture_type;
    unsigned int mime_type_length;
    char *mime_type;
    unsigned int description_length;
    unsigned char *description;
    unsigned int picture_width;
    unsigned int picture_height;
    unsigned int color_depth;
    unsigned int number_of_indexed_color;
    unsigned int picture_length;
    char picture_path[MAXPATHLENGTH];
    unsigned char *picture_data;
    FILE *picture_fp;    
    
    do {
        fread(&block_header, 1, 1, fp);
        fread(length, 1, 3, fp);
        ilength = (unsigned int) ((length[0]<<16)|(length[1]<<8)|(length[2]));
        switch(0x7F&block_header) {
            case 0:
                if(verbose) printf(" STREAMINFO\n");
                fseek(fp, ilength, SEEK_CUR);
                break;
            case 1:
                if(verbose) printf(" PADDING\n");
                fseek(fp, ilength, SEEK_CUR);
                break;
            case 2:
                if(verbose) printf(" APPLICATION\n");
                fseek(fp, ilength, SEEK_CUR);
                break;
            case 3:
                if(verbose) printf(" SEEKTABLE\n");
                fseek(fp, ilength, SEEK_CUR);
                break;
            case 4:
                if(verbose) printf(" VORBIS_COMMENT\n");
                fseek(fp, ilength, SEEK_CUR);
                break;
            case 5:
                if(verbose) printf(" CUESHEET\n");
                fseek(fp, ilength, SEEK_CUR);
                break;
            case 6: /* Picture data */
                if(verbose) printf(" PICTURE\n");
                fread(buf, 1, 4, fp);
                picture_type = ((buf[0]<<24)|(buf[1]<<16)|(buf[2]<<8)|buf[3]);
                /* if Picture type is 3, this is Front Cover picture */
                if(verbose) printf("  Picture type = %d\n", picture_type);
                fread(buf, 1, 4, fp);
                mime_type_length = ((buf[0]<<24)|(buf[1]<<16)|(buf[2]<<8)|buf[3]);
                if(verbose) printf("  MIME type length = %d\n", mime_type_length);
                
                mime_type = (char *) malloc(mime_type_length + 1);
                fread(mime_type, 1, mime_type_length, fp);
                mime_type[mime_type_length] = '\0';
                if(verbose) printf("  MIME type = %s\n", mime_type);
                
                fread(buf, 1, 4, fp);
                description_length = ((buf[0]<<24)|(buf[1]<<16)|(buf[2]<<8)|buf[3]);
                if(verbose) printf("  Description length = %d\n", description_length);
                
                description = (unsigned char *) malloc(description_length);
                fread(mime_type, 1, description_length, fp);
                if(verbose) printf("  Description = %s\n", description);
                free(description);
                
                fread(buf, 1, 4, fp);
                picture_width = ((buf[0]<<24)|(buf[1]<<16)|(buf[2]<<8)|buf[3]);
                if(verbose) printf("  Picture width = %d\n", picture_width);
                
                fread(buf, 1, 4, fp);
                picture_height = ((buf[0]<<24)|(buf[1]<<16)|(buf[2]<<8)|buf[3]);
                if(verbose) printf("  Picture height = %d\n", picture_height);
                
                fread(buf, 1, 4, fp);
                color_depth = ((buf[0]<<24)|(buf[1]<<16)|(buf[2]<<8)|buf[3]);
                if(verbose) printf("  Color Depth = %d\n", color_depth);
                
                fread(buf, 1, 4, fp);
                number_of_indexed_color = ((buf[0]<<24)|(buf[1]<<16)|(buf[2]<<8)|buf[3]);
                if(verbose) printf("  Number of colors for indexed-color = %d\n", number_of_indexed_color);
                fread(buf, 1, 4, fp);
                
                picture_length = ((buf[0]<<24)|(buf[1]<<16)|(buf[2]<<8)|buf[3]);
                if(verbose) printf("  Length of picture data = %d\n", picture_length);
                
                strcpy(picture_path, outputFilepath);
                //strcat(picture_path, "/");

                /* check if cover-art file already exists */
                if((picture_fp = fopen(picture_path, "r")) != NULL) {
                    
                    /*if(verbose) printf("  File %s exists, did not override.\n", picture_path);
                    if(!override) {
                        fclose(picture_fp);
                        free(mime_type);
                        return DONE;
                    }*/
                }
                /* some ripper software create blank PICTURE frame :-( */
                if(picture_length == 0) {
                    fprintf(stderr, "  Warning: picture size is 0, will not create JPEG file.\n");
                    break;
                }
                /* read picture date and write it to a file */
                picture_data = (unsigned char*)malloc(picture_length);
                fread(picture_data, 1, picture_length, fp);
                picture_fp = fopen(picture_path, "w");
                if(picture_fp == NULL) {
                    perror("get_FLAC_cover()::fopen()");
                    return DONE;
                }
                fwrite(picture_data, 1, picture_length, picture_fp);
                if(verbose) printf("  Wrote picture to %s\n", picture_path);
                fclose(picture_fp);
                free(picture_data);
                free(mime_type);
                return DONE;
                
            default:
                if(verbose) printf(" other metadata\n");
                fseek(fp, ilength, SEEK_CUR);
        }
    } while ((0x80&block_header) == 0);
    
    return NOTYET;
}


/*============================================================================
 Function for extracting JPEG/PNG data from a m4a file
 ============================================================================*/
static int get_m4a_cover(FILE *fp, const char *outputFilepath)
{
    unsigned char coffset[5], boxtype[5];
    unsigned int offset1, offset2, offset3, offset4, offset5, covrdata_len;
    unsigned int sum2, sum3, sum4, sum5;
    int flag;
    size_t size;
    char picture_path[MAXPATHLENGTH];
    char flag_data[8];
    unsigned char *picture_data;
    FILE *picture_fp;
    
    rewind(fp);
    while((size = fread(coffset, 1, 4, fp))>0) {
        offset1 = ((coffset[0]<<24)|(coffset[1]<<16)|(coffset[2]<<8)|coffset[3]);
        size = fread(boxtype, 1, 4, fp);
        boxtype[4] = 0;
        if(memcmp(boxtype,"moov",4)==0) { /* look into moov box */
            sum2 = 0;
            do {
                size = fread(coffset, 1, 4, fp);
                offset2 = ((coffset[0]<<24)|(coffset[1]<<16)|(coffset[2]<<8)|coffset[3]);
                sum2 += offset2;
                size = fread(boxtype, 1, 4, fp);
                boxtype[4] = 0;
                if(verbose) printf(" box type = moov:%s\n", boxtype);
                if(memcmp(boxtype, "udta", 4)==0) { /* look into udta box */
                    sum3 = 0;
                    do {
                        size = fread(coffset, 1, 4, fp);
                        offset3 = ((coffset[0]<<24)|(coffset[1]<<16)|(coffset[2]<<8)|coffset[3]);
                        sum3 += offset3;
                        size = fread(boxtype, 1, 4, fp);
                        boxtype[4] = 0;
                        if(verbose) printf(" box type = moov:udta:%s\n", boxtype);
                        if(memcmp(boxtype, "meta", 4)==0) { /* look into meta */
                            sum4 = 0;
                            do {
                                size = fread(coffset, 1, 4, fp);
                                offset4 = ((coffset[0]<<24)|(coffset[1]<<16)|(coffset[2]<<8)|coffset[3]);
                                if(offset4 == 0) { /* one byte atom version */
                                    size = fread(coffset, 1, 4, fp);
                                    offset4 = ((coffset[0]<<24)|(coffset[1]<<16)|(coffset[2]<<8)|coffset[3]);
                                }
                                size = fread(boxtype, 1, 4, fp);
                                boxtype[4] = 0;
                                sum4 += offset4;
                                if(verbose) printf(" box type = moov:udta:meta:%s\n", boxtype);
                                if(memcmp(boxtype, "ilst", 4)==0) { /* look into ilst */
                                    do {
                                        sum5 = 0;
                                        size = fread(coffset, 1, 4, fp);
                                        offset5 = ((coffset[0]<<24)|(coffset[1]<<16)|(coffset[2]<<8)|coffset[3]);
                                        if(offset5 == 0) { /* one byte atom version */
                                            size = fread(coffset, 1, 4, fp);
                                            offset5 = ((coffset[0]<<24)|(coffset[1]<<16)|(coffset[2]<<8)|coffset[3]);
                                        }
                                        size = fread(boxtype, 1, 4, fp);
                                        boxtype[4] = 0;
                                        sum5 += offset5;
                                        if(verbose) printf(" box type = moov:udta:meta:ilst:%s\n", boxtype);
                                        if(memcmp(boxtype, "covr", 4)==0) { /* look into cover */
                                            size = fread(coffset, 1, 4, fp);
                                            covrdata_len = ((coffset[0]<<24)|(coffset[1]<<16)|(coffset[2]<<8)|coffset[3]);
                                            size = fread(boxtype, 1, 4, fp);
                                            boxtype[4] = 0;
                                            if(verbose) printf(" box type = moov:udta:meta:ilst:covr:%s\n", boxtype);
                                            
                                            strcpy(picture_path, outputFilepath);
                                            //strcat(picture_path, "/");
                                            size = fread(flag_data, 1, 8, fp);
                                            flag = flag_data[3];
                                            if(flag == 14) {
                                               // strcat(picture_path, pngFile);
                                                fprintf(stderr, "  Warning: Image type is PNG not JPEG, generating Folder.png\n");
                                            }
                                            else if(flag == 13)
                                            {
                                              //  strcat(picture_path, jpegFile);
                                            } else {
                                              //  strcat(picture_path, jpegFile);
                                                fprintf(stderr, "  Warning: unknown image format:%d, generating %s\n", flag, jpegFile);
                                            }
                                            /* check if cover-ar file already exists */
                                            if((picture_fp = fopen(picture_path, "r")) != NULL) {
                                                if(verbose) printf("  File %s exists, did not override.\n", picture_path);
                                                /*if(!override) {
                                                    fclose(picture_fp);
                                                    return DONE;
                                                }*/
                                            }
                                            /* some ripper software create blank PICTURE frame :-( */
                                            if(covrdata_len == 0) {
                                                fprintf(stderr, "  Warning: picture size is 0, will not create JPEG file.\n");
                                                return NOTYET;
                                            }
                                            /* read picture date and write it to a file */
                                            picture_data = (unsigned char*)malloc(covrdata_len);
                                            fread(picture_data, 1, covrdata_len-8, fp);
                                            picture_fp = fopen(picture_path, "w");
                                            if(picture_fp == NULL) {
                                                perror("get_m4a_cover()::fopen()");
                                                return DONE;
                                            }
                                            fwrite(picture_data, 1, covrdata_len, picture_fp);
                                            if(verbose) printf("  Wrote picture to %s\n", picture_path);
                                            fclose(picture_fp);
                                            free(picture_data);
                                            return DONE;
                                        } /* end of cover search */
                                        fseek(fp, offset5-8, SEEK_CUR);
                                    } while(sum5 < offset4);
                                    return NOTYET; /* cover not found in ilst */
                                } /* end of ilst search */
                                fseek(fp, offset4-8, SEEK_CUR);
                            } while(sum4 < offset3);
                        } /* end of meta search */
                        fseek(fp, offset3-8, SEEK_CUR);
                    } while(sum3 < offset2);
                } /* end of udta search */
                else fseek(fp, offset2-8, SEEK_CUR);
            } while(sum2 < offset1);
        } /* end of moov search */
        else fseek(fp, offset1-8, SEEK_CUR);
    }
    return NOTYET;
}

#endif  /* GETCOVER_H */