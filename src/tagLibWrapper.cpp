// taglib_wrapper.cpp
#include <taglib/fileref.h>
#include <taglib/mpegfile.h>
#include <taglib/id3v2tag.h>
#include <taglib/id3v2frame.h>
#include <taglib/id3v2header.h>
#include <taglib/flacfile.h>
#include <taglib/mp4file.h>
#include <taglib/mp4coverart.h>
#include <taglib/oggfile.h>
#include <taglib/vorbisfile.h>
#include <taglib/opusfile.h>
#include <taglib/tbytevector.h>
#include <taglib/tpropertymap.h>
#include <taglib/tstringlist.h>
#include <taglib/tvariant.h>
#include <taglib/fileref.h>
#include <taglib/tag.h>
#include <cstring>
#include <cstdio>
#include <vector>
#include <string>
#include "tagLibWrapper.h"

extern "C"
{
        // Simple Base64 decoding function for Ogg Vorbis/Opus cover art
        std::vector<unsigned char> decodeBase64(const std::string &data)
        {
                static const char lookup[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
                std::vector<unsigned char> decodedData;

                unsigned int buffer = 0;
                int bits = 0;

                for (char ch : data)
                {
                        if (ch == '=')
                                break;

                        const char *pos = std::strchr(lookup, ch);
                        if (!pos)
                                continue;

                        buffer = (buffer << 6) | (pos - lookup);
                        bits += 6;

                        if (bits >= 8)
                        {
                                bits -= 8;
                                decodedData.push_back((buffer >> bits) & 0xFF);
                        }
                }

                return decodedData;
        }

        void trimcpp(std::string &str)
        {
                // Remove leading spaces
                str.erase(str.begin(), std::find_if(str.begin(), str.end(), [](unsigned char ch)
                                                    { return !std::isspace(ch); }));

                // Remove trailing spaces
                str.erase(std::find_if(str.rbegin(), str.rend(), [](unsigned char ch)
                                       { return !std::isspace(ch); })
                              .base(),
                          str.end());
        }

        void turnFilePathIntoTitle(const char *filePath, char *title, size_t titleMaxLength)
        {
                std::string filePathStr(filePath); // Convert the C-style string to std::string

                size_t lastSlashPos = filePathStr.find_last_of("/\\"); // Find the last '/' or '\\'
                size_t lastDotPos = filePathStr.find_last_of('.');     // Find the last '.'

                // Validate that both positions exist and the dot is after the last slash
                if (lastSlashPos != std::string::npos && lastDotPos != std::string::npos && lastDotPos > lastSlashPos)
                {
                        // Extract the substring between the last slash and the last dot
                        std::string extractedTitle = filePathStr.substr(lastSlashPos + 1, lastDotPos - lastSlashPos - 1);

                        // Trim any unwanted spaces
                        trimcpp(extractedTitle);

                        // Ensure title is not longer than titleMaxLength, including the null terminator
                        if (extractedTitle.length() >= titleMaxLength)
                        {
                                extractedTitle = extractedTitle.substr(0, titleMaxLength - 1);
                        }

                        // Copy the result into the output char* title, ensuring no overflow
                        strncpy(title, extractedTitle.c_str(), titleMaxLength - 1); // Copy up to titleMaxLength - 1 characters
                        title[titleMaxLength - 1] = '\0';                           // Null-terminate the string
                }
                else
                {
                        // If no valid title is found, ensure title is an empty string
                        title[0] = '\0';
                }
        }

        int extractTags(const char *input_file, TagSettings *tag_settings, double *duration, const char *coverFilePath)
        {
                memset(tag_settings, 0, sizeof(TagSettings)); // Initialize tag settings

                // Use TagLib's FileRef for generic file parsing.
                TagLib::FileRef f(input_file);
                if (f.isNull() || !f.file())
                {
                        fprintf(stderr, "FileRef is null or file could not be opened: '%s'\n", input_file);
                        return -2;
                }

                // Extract tags using the stable method that worked before.
                TagLib::Tag *tag = f.tag();
                if (!tag)
                {
                        fprintf(stderr, "Tag is null for file '%s'\n", input_file);
                        return -2;
                }

                // Copy the title
                strncpy(tag_settings->title, tag->title().toCString(true), sizeof(tag_settings->title) - 1);
                tag_settings->title[sizeof(tag_settings->title) - 1] = '\0';

                // Check if the title is empty, and if so, use the file path to generate a title
                if (strlen(tag_settings->title) <= 0)
                {
                        char title[4096];
                        turnFilePathIntoTitle(input_file, title, 4096);
                        strncpy(tag_settings->title, title, sizeof(tag_settings->title) - 1);
                        tag_settings->title[sizeof(tag_settings->title) - 1] = '\0';
                }
                else
                {
                        // Copy the artist
                        strncpy(tag_settings->artist, tag->artist().toCString(true), sizeof(tag_settings->artist) - 1);
                        tag_settings->artist[sizeof(tag_settings->artist) - 1] = '\0';

                        // Copy the album
                        strncpy(tag_settings->album, tag->album().toCString(true), sizeof(tag_settings->album) - 1);
                        tag_settings->album[sizeof(tag_settings->album) - 1] = '\0';

                        // Copy the year as date
                        snprintf(tag_settings->date, sizeof(tag_settings->date), "%d", tag->year());
                }
                
                // Extract audio properties for duration.
                if (f.audioProperties())
                {
                        *duration = f.audioProperties()->lengthInSeconds();
                }
                else
                {
                        *duration = 0.0;
                        fprintf(stderr, "No audio properties found for file '%s'\n", input_file);
                        return -2;
                }

                // Cover art extraction using format-specific classes if necessary.
                std::string filename(input_file);
                std::string extension = filename.substr(filename.find_last_of('.') + 1);
                bool coverArtExtracted = false;

                if (extension == "mp3")
                {
                        TagLib::MPEG::File file(input_file);
                        if (!file.isValid())
                        {
                                fprintf(stderr, "Could not open input file '%s'\n", input_file);
                                return -2;
                        }
                        tag = file.tag();

                        // Extract cover art from ID3v2 tags
                        TagLib::ID3v2::Tag *id3v2tag = file.ID3v2Tag();
                        if (id3v2tag)
                        {
                                TagLib::ID3v2::FrameList frames = id3v2tag->frameList("APIC");
                                if (!frames.isEmpty())
                                {
                                        TagLib::ID3v2::Frame *frame = frames.front();
                                        if (frame)
                                        {
                                                TagLib::ByteVector data = frame->render();
                                                size_t offset = 0;
                                                while (offset < data.size() && data[offset] != 0x00)
                                                {
                                                        offset++;
                                                }
                                                offset++;
                                                FILE *file = fopen(coverFilePath, "wb");
                                                if (file)
                                                {
                                                        fwrite(data.data() + offset, 1, data.size() - offset, file);
                                                        fclose(file);
                                                        coverArtExtracted = true;
                                                }
                                                else
                                                {
                                                        fprintf(stderr, "Could not open output file '%s'\n", coverFilePath);
                                                        return -1;
                                                }
                                        }
                                }
                                else
                                {
                                        return -1;
                                }
                        }
                }
                else if (extension == "flac")
                {
                        // Use TagLib::FLAC::File only for extracting cover art.
                        TagLib::FLAC::File file(input_file);
                        if (file.pictureList().size() > 0)
                        {
                                TagLib::FLAC::Picture *picture = file.pictureList().front();
                                if (picture)
                                {
                                        FILE *file = fopen(coverFilePath, "wb");
                                        if (file)
                                        {
                                                fwrite(picture->data().data(), 1, picture->data().size(), file);
                                                fclose(file);
                                                coverArtExtracted = true;
                                        }
                                        else
                                        {
                                                return -1;
                                        }
                                }
                        }
                        else
                        {
                                return -1;
                        }
                }
                else if (extension == "m4a" || extension == "aac")
                {
                        TagLib::MP4::File file(input_file);
                        const TagLib::MP4::Item coverItem = file.tag()->item("covr");
                        if (coverItem.isValid())
                        {
                                TagLib::MP4::CoverArtList coverArtList = coverItem.toCoverArtList();
                                if (!coverArtList.isEmpty())
                                {
                                        const TagLib::MP4::CoverArt &coverArt = coverArtList.front();
                                        FILE *file = fopen(coverFilePath, "wb");
                                        if (file)
                                        {
                                                fwrite(coverArt.data().data(), 1, coverArt.data().size(), file);
                                                fclose(file);
                                                coverArtExtracted = true;
                                        }
                                        else
                                        {
                                                fprintf(stderr, "Could not open output file '%s'\n", coverFilePath);
                                                return -1;
                                        }
                                }
                        }
                        else
                        {
                                return -1;
                        }
                }
                else if (extension == "ogg" || extension == "opus")
                {
                        TagLib::Ogg::Vorbis::File vorbisFile(input_file);
                        if (vorbisFile.isValid())
                        {
                                TagLib::Ogg::FieldListMap comments = vorbisFile.tag()->fieldListMap();
                                if (comments.contains("METADATA_BLOCK_PICTURE"))
                                {
                                        std::string encodedData = comments["METADATA_BLOCK_PICTURE"].front().to8Bit(true);
                                        std::vector<unsigned char> pictureData = decodeBase64(encodedData);
                                        FILE *file = fopen(coverFilePath, "wb");
                                        if (file)
                                        {
                                                fwrite(pictureData.data(), 1, pictureData.size(), file);
                                                fclose(file);
                                                coverArtExtracted = true;
                                        }
                                        else
                                        {
                                                fprintf(stderr, "Could not open output file '%s'\n", coverFilePath);
                                                return -1;
                                        }
                                }
                                else
                                {
                                        return -1;
                                }
                        }
                }
                // Return success codes based on cover art extraction.
                if (coverArtExtracted)
                {
                        return 0; // Success with cover art.
                }
                else
                {
                        return -1; // No cover art.
                }
        }
}
