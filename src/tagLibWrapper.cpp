// taglib_wrapper.cpp
#include <algorithm>
#include <map>
#include <cctype>
#include <cstring>
#include <cstdio>
#include <vector>
#include <string>
#include <cstring>
#include <fstream>
#include <iostream>

#include <taglib/attachedpictureframe.h>
#include <taglib/fileref.h>
#include <taglib/mpegfile.h>
#include <taglib/id3v2tag.h>
#include <taglib/id3v2frame.h>
#include <taglib/flacfile.h>
#include <taglib/mp4file.h>
#include <taglib/mp4coverart.h>
#include <taglib/vorbisfile.h>
#include <taglib/opusfile.h>
#include <taglib/xiphcomment.h>
#include <taglib/tag.h>
#include <ogg/ogg.h>

/*

tagLibWrapper.cpp

 Related to extracting meta tags and cover from audio files.

*/

#if defined(__linux__)
#include <opus/opusfile.h>
#else
#include <opusfile.h>
#endif
#include "tagLibWrapper.h"

// Base64 character map for decoding
static const std::string base64_chars =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789+/";

static inline bool is_base64(unsigned char c)
{
        return (isalnum(c) || (c == '+') || (c == '/'));
}

// Function to decode Base64-encoded data
std::vector<unsigned char> decodeBase64(const std::string &encoded_string)
{
        size_t in_len = encoded_string.size();
        size_t i = 0;
        size_t in_ = 0;
        unsigned char char_array_4[4], char_array_3[3];
        std::vector<unsigned char> decoded_data;

        while (in_len-- && (encoded_string[in_] != '=') && is_base64(encoded_string[in_]))
        {
                char_array_4[i++] = encoded_string[in_];
                in_++;
                if (i == 4)
                {
                        for (i = 0; i < 4; i++)
                                char_array_4[i] = static_cast<unsigned char>(base64_chars.find(char_array_4[i]));

                        char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
                        char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
                        char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

                        for (i = 0; i < 3; i++)
                                decoded_data.push_back(char_array_3[i]);

                        i = 0;
                }
        }

        if (i)
        {
                for (size_t j = i; j < 4; j++)
                        char_array_4[j] = 0;

                for (size_t j = 0; j < 4; j++)
                        char_array_4[j] = static_cast<unsigned char>(base64_chars.find(char_array_4[j]));

                char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
                char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
                char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

                for (size_t j = 0; j < i - 1; j++)
                        decoded_data.push_back(char_array_3[j]);
        }

        return decoded_data;
}

extern "C"
{
        // Function to read a 32-bit unsigned integer from buffer in big-endian format
        unsigned int read_uint32_be(const unsigned char *buffer, size_t offset)
        {
                return (static_cast<unsigned int>(buffer[offset]) << 24) |
                       (static_cast<unsigned int>(buffer[offset + 1]) << 16) |
                       (static_cast<unsigned int>(buffer[offset + 2]) << 8) |
                       static_cast<unsigned int>(buffer[offset + 3]);
        }

        void parseFlacPictureBlock(const std::vector<unsigned char> &data,
                                   std::string &mimeType,
                                   std::vector<unsigned char> &imageData)
        {
                const unsigned char *ptr = data.data();
                size_t offset = 0;

                auto readUInt32 = [&](uint32_t &value)
                {
                        value = (ptr[offset] << 24) | (ptr[offset + 1] << 16) |
                                (ptr[offset + 2] << 8) | ptr[offset + 3];
                        offset += 4;
                };

                uint32_t pictureType, mimeLength, descLength, width, height, depth, colors, dataLength;
                readUInt32(pictureType);
                readUInt32(mimeLength);

                mimeType = std::string(reinterpret_cast<const char *>(&ptr[offset]), mimeLength);
                offset += mimeLength;

                readUInt32(descLength);
                offset += descLength; // Skip description

                readUInt32(width);
                readUInt32(height);
                readUInt32(depth);
                readUInt32(colors);
                readUInt32(dataLength);

                imageData.assign(&ptr[offset], &ptr[offset + dataLength]);
        }

        int extractCoverArtFromOgg(const std::string &audioFilePath, const std::string &outputFileName)
        {
                TagLib::File *file = nullptr;
                TagLib::Tag *tag = nullptr;

                // Try to open as Ogg Vorbis
                file = new TagLib::Vorbis::File(audioFilePath.c_str());
                if (!file->isValid())
                {
                        delete file;
                        // Try to open as Opus
                        file = new TagLib::Ogg::Opus::File(audioFilePath.c_str());
                        if (!file->isValid())
                        {
                                delete file;
                                std::cerr << "Error: File not found or not a valid Ogg Vorbis or Opus file." << std::endl;
                                return false; // File not found or invalid
                        }
                }

                tag = file->tag();

                const TagLib::Ogg::XiphComment *xiphComment = dynamic_cast<TagLib::Ogg::XiphComment *>(tag);
                if (!xiphComment)
                {
                        std::cerr << "Error: No XiphComment found in the file." << std::endl;
                        delete file;
                        return false; // No cover art found
                }

                // Check METADATA_BLOCK_PICTURE
                TagLib::StringList pictureList = xiphComment->fieldListMap()["METADATA_BLOCK_PICTURE"];
                if (!pictureList.isEmpty())
                {
                        std::string base64Data = pictureList.front().to8Bit(true);
                        std::vector<unsigned char> decodedData = decodeBase64(base64Data);

                        std::string mimeType;
                        std::vector<unsigned char> imageData;
                        parseFlacPictureBlock(decodedData, mimeType, imageData);

                        std::ofstream outFile(outputFileName, std::ios::binary);
                        if (!outFile)
                        {
                                std::cerr << "Error: Could not write to output file." << std::endl;
                                delete file;
                                return false; // Could not write to output file
                        }
                        outFile.write(reinterpret_cast<const char *>(imageData.data()), imageData.size());
                        outFile.close();
                        delete file;
                        return 0; // Success
                }

                // Check COVERART and COVERARTMIME
                TagLib::StringList coverArtList = xiphComment->fieldListMap()["COVERART"];
                TagLib::StringList coverArtMimeList = xiphComment->fieldListMap()["COVERARTMIME"];
                if (!coverArtList.isEmpty() && !coverArtMimeList.isEmpty())
                {
                        std::string base64Data = coverArtList.front().to8Bit(true);
                        std::vector<unsigned char> imageData = decodeBase64(base64Data);

                        std::ofstream outFile(outputFileName, std::ios::binary);
                        if (!outFile)
                        {
                                std::cerr << "Error: Could not write to output file." << std::endl;
                                delete file;
                                return false; // Could not write to output file
                        }
                        outFile.write(reinterpret_cast<const char *>(imageData.data()), imageData.size());
                        outFile.close();
                        delete file;
                        return true; // Success
                }

                std::cerr << "No cover art found in the file." << std::endl;
                delete file;
                return false; // No cover art found
        }

        bool extractCoverArtFromOggVideo(const std::string &audioFilePath, const std::string &outputFileName)
        {
                FILE *oggFile = fopen(audioFilePath.c_str(), "rb");
                if (!oggFile)
                {
                        std::cerr << "Error: Could not open file." << std::endl;
                        return false; // File not found or could not be opened
                }

                ogg_sync_state oy;
                ogg_sync_init(&oy);

                ogg_page og;
                ogg_packet op;

                std::map<int, ogg_stream_state> streams;
                std::map<int, std::vector<unsigned char>> imageDataStreams;
                std::map<int, bool> isImageStream; // Track if a stream is an image (JPEG/PNG)

                char *buffer;
                int bytes;
                bool coverArtFound = false;

                // Read through the Ogg container to find and extract PNG or JPEG streams
                while (true)
                {
                        buffer = ogg_sync_buffer(&oy, 4096);
                        bytes = fread(buffer, 1, 4096, oggFile);
                        ogg_sync_wrote(&oy, bytes);

                        while (ogg_sync_pageout(&oy, &og) == 1)
                        {
                                int serialNo = ogg_page_serialno(&og);

                                // Initialize stream if not already done
                                if (streams.find(serialNo) == streams.end())
                                {
                                        ogg_stream_state os;
                                        ogg_stream_init(&os, serialNo);
                                        streams[serialNo] = os;
                                        isImageStream[serialNo] = false; // Start by assuming this is not an image stream
                                }

                                ogg_stream_state &os = streams[serialNo];
                                ogg_stream_pagein(&os, &og);

                                while (ogg_stream_packetout(&os, &op) == 1)
                                {
                                        // Check the packet for PNG signature (\x89PNG\r\n\x1A\n) or JPEG SOI (\xFF\xD8)
                                        if (op.bytes >= 8 && std::memcmp(op.packet, "\x89PNG\r\n\x1A\n", 8) == 0)
                                        {
                                                isImageStream[serialNo] = true;
                                        }
                                        else if (op.bytes >= 2 && op.packet[0] == 0xFF && op.packet[1] == 0xD8)
                                        {
                                                isImageStream[serialNo] = true;
                                        }
                                        else if (op.bytes >= 12 && std::memcmp(op.packet, "RIFF", 4) == 0 && std::memcmp(op.packet + 8, "WEBP", 4) == 0)
                                        {
                                                isImageStream[serialNo] = true;
                                        }

                                        // If this is a detected image stream (either PNG or JPEG), collect the packet data
                                        if (isImageStream[serialNo])
                                        {
                                                imageDataStreams[serialNo].insert(
                                                    imageDataStreams[serialNo].end(),
                                                    op.packet,
                                                    op.packet + op.bytes);
                                        }
                                }
                        }

                        // Stop reading if the file ends
                        if (bytes == 0)
                        {
                                break;
                        }
                }

                fclose(oggFile);
                ogg_sync_clear(&oy);

                // Process collected image data streams
                for (const auto &entry : imageDataStreams)
                {
                        if (isImageStream[entry.first] && !entry.second.empty())
                        {
                                // Save the image data to a file using outputFileName
                                std::ofstream outFile(outputFileName, std::ios::binary);
                                if (!outFile)
                                {
                                        std::cerr << "Error: Could not write to output file." << std::endl;
                                        // Clean up stream states
                                        for (auto &streamEntry : streams)
                                        {
                                                ogg_stream_clear(&streamEntry.second);
                                        }
                                        return false; // Could not write to output file
                                }
                                outFile.write(reinterpret_cast<const char *>(entry.second.data()), entry.second.size());
                                outFile.close();

                                coverArtFound = true;
                                break; // Stop after finding and writing the first cover art
                        }
                }

                // Clean up stream states
                for (auto &streamEntry : streams)
                {
                        ogg_stream_clear(&streamEntry.second);
                }

                // Return whether the cover art was successfully found and written
                if (!coverArtFound)
                {
                        std::cerr << "No cover art found in the file." << std::endl;
                        return false; // No cover art found
                }

                return true; // Success
        }

        bool extractCoverArtFromMp3(const std::string &inputFile, const std::string &coverFilePath)
        {
                TagLib::MPEG::File file(inputFile.c_str());
                if (!file.isValid())
                {
                        return false;
                }

                const TagLib::ID3v2::Tag *id3v2tag = file.ID3v2Tag();
                if (id3v2tag)
                {
                        // Collect all attached picture frames
                        TagLib::ID3v2::FrameList frames;
                        frames.append(id3v2tag->frameListMap()["APIC"]);
                        frames.append(id3v2tag->frameListMap()["PIC"]);

                        if (!frames.isEmpty())
                        {
                                for (auto it = frames.begin(); it != frames.end(); ++it)
                                {
                                        const TagLib::ID3v2::AttachedPictureFrame *picFrame = dynamic_cast<TagLib::ID3v2::AttachedPictureFrame *>(*it);
                                        if (picFrame)
                                        {
                                                // Access picture data and MIME type
                                                TagLib::ByteVector pictureData = picFrame->picture();
                                                TagLib::String mimeType = picFrame->mimeType();

                                                // Construct the output file path
                                                std::string outputFilePath = coverFilePath;

                                                // Write the image data to a file
                                                FILE *outFile = fopen(outputFilePath.c_str(), "wb");
                                                if (outFile)
                                                {
                                                        fwrite(pictureData.data(), 1, pictureData.size(), outFile);
                                                        fclose(outFile);

                                                        return true;
                                                }
                                                else
                                                {
                                                        return false; // Failed to open output file
                                                }

                                                // Break if only the first image is needed
                                                break;
                                        }
                                }
                        }
                        else
                        {
                                return false; // No picture frames found
                        }
                }
                else
                {
                        return false; // No ID3v2 tag found
                }

                return true; // Success
        }

        bool extractCoverArtFromFlac(const std::string &inputFile, const std::string &coverFilePath)
        {
                TagLib::FLAC::File file(inputFile.c_str());

                if (file.pictureList().size() > 0)
                {
                        const TagLib::FLAC::Picture *picture = file.pictureList().front();
                        if (picture)
                        {
                                FILE *coverFile = fopen(coverFilePath.c_str(), "wb");
                                if (coverFile)
                                {
                                        fwrite(picture->data().data(), 1, picture->data().size(), coverFile);
                                        fclose(coverFile);
                                        return true;
                                }
                                else
                                {
                                        return false;
                                }
                        }
                }

                return false;
        }

        bool extractCoverArtFromOpus(const std::string &audioFilePath, const std::string &outputFileName)
        {
                int error;
                OggOpusFile *of = op_open_file(audioFilePath.c_str(), &error);
                if (error != OPUS_OK || of == nullptr)
                {
                        std::cerr << "Error: Failed to open Opus file." << std::endl;
                        return false;
                }

                const OpusTags *tags = op_tags(of, -1);
                if (!tags)
                {
                        std::cerr << "Error: No tags found in Opus file." << std::endl;
                        op_free(of);
                        return false;
                }

                // Search through the metadata for an attached picture (if present)
                for (int i = 0; i < tags->comments; ++i)
                {
                        // Check for METADATA_BLOCK_PICTURE
                        const char *comment = tags->user_comments[i];
                        if (strncmp(comment, "METADATA_BLOCK_PICTURE=", 23) == 0)
                        {
                                // Extract the value after "METADATA_BLOCK_PICTURE="
                                std::string metadataBlockPicture(comment + 23);

                                // Base64-decode this value to get the binary PICTURE block
                                std::vector<unsigned char> pictureBlock = decodeBase64(metadataBlockPicture);

                                if (pictureBlock.empty())
                                {
                                        std::cerr << "Failed to decode Base64 data." << std::endl;
                                        op_free(of);
                                        return false;
                                }

                                // Now parse the binary pictureBlock to extract the image data
                                size_t offset = 0;

                                if (pictureBlock.size() < 32)
                                {
                                        std::cerr << "Picture block too small." << std::endl;
                                        op_free(of);
                                        return false;
                                }

                                // Read PICTURE TYPE
                                read_uint32_be(pictureBlock.data(), offset);
                                offset += 4;

                                // Read MIME TYPE LENGTH
                                unsigned int mimeTypeLength = read_uint32_be(pictureBlock.data(), offset);
                                offset += 4;

                                // Read MIME TYPE
                                if (offset + mimeTypeLength > pictureBlock.size())
                                {
                                        op_free(of);
                                        return false;
                                }
                                offset += mimeTypeLength;

                                // Read DESCRIPTION LENGTH
                                unsigned int descriptionLength = read_uint32_be(pictureBlock.data(), offset);
                                offset += 4;

                                // Read DESCRIPTION
                                if (offset + descriptionLength > pictureBlock.size())
                                {
                                        op_free(of);
                                        return false;
                                }
                                offset += descriptionLength;
                                // Optionally print or ignore description

                                // Read WIDTH
                                read_uint32_be(pictureBlock.data(), offset);
                                offset += 4;

                                // Read HEIGHT
                                read_uint32_be(pictureBlock.data(), offset);
                                offset += 4;
                                ;

                                // Read COLOR DEPTH
                                read_uint32_be(pictureBlock.data(), offset);
                                offset += 4;

                                // Read NUMBER OF COLORS
                                read_uint32_be(pictureBlock.data(), offset);
                                offset += 4;

                                // Read DATA LENGTH
                                unsigned int dataLength = read_uint32_be(pictureBlock.data(), offset);
                                offset += 4;

                                if (offset + dataLength > pictureBlock.size())
                                {
                                        std::cerr << "Invalid image data length." << std::endl;
                                        op_free(of);
                                        return false;
                                }

                                // Extract image data
                                std::vector<unsigned char> imageData(pictureBlock.begin() + offset, pictureBlock.begin() + offset + dataLength);

                                // Save image data to file
                                std::ofstream outFile(outputFileName, std::ios::binary);
                                if (!outFile)
                                {
                                        std::cerr << "Error: Could not write to output file." << std::endl;
                                        op_free(of);
                                        return false;
                                }
                                outFile.write(reinterpret_cast<const char *>(imageData.data()), imageData.size());
                                outFile.close();

                                op_free(of);
                                return true;
                        }
                }

                std::cerr << "No cover art found in the metadata." << std::endl;
                op_free(of);
                return false;
        }

        bool extractCoverArtFromMp4(const std::string &inputFile, const std::string &coverFilePath)
        {
                TagLib::MP4::File file(inputFile.c_str());
                const TagLib::MP4::Item coverItem = file.tag()->item("covr");

                if (coverItem.isValid())
                {
                        TagLib::MP4::CoverArtList coverArtList = coverItem.toCoverArtList();
                        if (!coverArtList.isEmpty())
                        {
                                const TagLib::MP4::CoverArt &coverArt = coverArtList.front();
                                FILE *coverFile = fopen(coverFilePath.c_str(), "wb");
                                if (coverFile)
                                {
                                        fwrite(coverArt.data().data(), 1, coverArt.data().size(), coverFile);
                                        fclose(coverFile);
                                        return true; // Success
                                }
                                else
                                {
                                        fprintf(stderr, "Could not open output file '%s'\n", coverFilePath.c_str());
                                        return false; // Failed to open the output file
                                }
                        }
                }

                return false; // No valid cover item or cover art found
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
                        c_strcpy(title, extractedTitle.c_str(), titleMaxLength - 1); // Copy up to titleMaxLength - 1 characters
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

                        char title[4096];
                        turnFilePathIntoTitle(input_file, title, 4096);
                        c_strcpy(tag_settings->title, title, sizeof(tag_settings->title) - 1);
                        tag_settings->title[sizeof(tag_settings->title) - 1] = '\0';

                        return -1;
                }

                // Extract tags using the stable method that worked before.
                const TagLib::Tag *tag = f.tag();
                if (!tag)
                {
                        fprintf(stderr, "Tag is null for file '%s'\n", input_file);
                        return -2;
                }

                // Copy the title
                c_strcpy(tag_settings->title, tag->title().toCString(true), sizeof(tag_settings->title) - 1);
                tag_settings->title[sizeof(tag_settings->title) - 1] = '\0';

                // Check if the title is empty, and if so, use the file path to generate a title
                if (strnlen(tag_settings->title, 10) == 0)
                {
                        char title[4096];
                        turnFilePathIntoTitle(input_file, title, 4096);
                        c_strcpy(tag_settings->title, title, sizeof(tag_settings->title) - 1);
                        tag_settings->title[sizeof(tag_settings->title) - 1] = '\0';
                }
                else
                {
                        // Copy the artist
                        c_strcpy(tag_settings->artist, tag->artist().toCString(true), sizeof(tag_settings->artist) - 1);
                        tag_settings->artist[sizeof(tag_settings->artist) - 1] = '\0';

                        // Copy the album
                        c_strcpy(tag_settings->album, tag->album().toCString(true), sizeof(tag_settings->album) - 1);
                        tag_settings->album[sizeof(tag_settings->album) - 1] = '\0';

                        // Copy the year as date
                        snprintf(tag_settings->date, sizeof(tag_settings->date), "%d", (int)tag->year());
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
                        coverArtExtracted = extractCoverArtFromMp3(input_file, coverFilePath);
                }
                else if (extension == "flac")
                {
                        coverArtExtracted = extractCoverArtFromFlac(input_file, coverFilePath);
                }
                else if (extension == "m4a" || extension == "aac")
                {
                        coverArtExtracted = extractCoverArtFromMp4(input_file, coverFilePath);
                }
                if (extension == "opus")
                {
                        coverArtExtracted = extractCoverArtFromOpus(input_file, coverFilePath);
                }
                else if (extension == "ogg")
                {
                        coverArtExtracted = extractCoverArtFromOggVideo(input_file, coverFilePath);

                        if (!coverArtExtracted)
                        {
                                coverArtExtracted = extractCoverArtFromOgg(input_file, coverFilePath);
                        }
                }

                if (coverArtExtracted)
                {
                        return 0;
                }
                else
                {
                        return -1;
                }
        }
}
