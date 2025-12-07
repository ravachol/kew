/**
 * @file taglibwrapper.cpp
 * @brief C++ wrapper around TagLib for metadata extraction.
 *
 * Provides a simplified interface for reading song metadata such as
 * title, artist, album and embedded artwork using the TagLib library.
 * Exposes a C-compatible API for integration with the main C codebase.
 */
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <ogg/ogg.h>
#include <string>
#include <taglib/apefooter.h>
#include <taglib/apeitem.h>
#include <taglib/apetag.h>
#include <taglib/attachedpictureframe.h>
#include <taglib/fileref.h>
#include <taglib/flacfile.h>
#include <taglib/id3v2frame.h>
#include <taglib/id3v2tag.h>
#include <taglib/mp4coverart.h>
#include <taglib/mp4file.h>
#include <taglib/mpegfile.h>
#include <taglib/opusfile.h>
#include <taglib/synchronizedlyricsframe.h>
#include <taglib/tag.h>
#include <taglib/textidentificationframe.h>
#include <taglib/vorbisfile.h>
#include <taglib/wavfile.h>
#include <taglib/xiphcomment.h>
#include <vector>

#if defined(__linux__)
#include <opus/opusfile.h>
#else
#include <opusfile.h>
#endif
#include "tagLibWrapper.h"

// Base64 character map for decoding
static const std::string base64_chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                        "abcdefghijklmnopqrstuvwxyz"
                                        "0123456789+/";

const uint32_t MAX_REASONABLE_SIZE = 100 * 1024 * 1024; // 100MB limit

#if defined(TAGLIB_MAJOR_VERSION) && TAGLIB_MAJOR_VERSION >= 2
#define HAVE_COMPLEXPROPERTIES 1
#else
#define HAVE_COMPLEXPROPERTIES 0
#endif

std::vector<unsigned char> decodeBase64(const std::string &encoded_string)
{
        const size_t MAX_DECODED_SIZE = 100 * 1024 * 1024; // 100 MB
        const size_t MAX_ENCODED_SIZE =
            (MAX_DECODED_SIZE * 4) / 3 + 4; // Max base64 size + padding
        const std::string base64_chars =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

        auto is_base64 = [&](unsigned char c) -> bool { return base64_chars.find(c) != std::string::npos; };

        auto base64_index = [&](unsigned char c) -> unsigned char {
                auto pos = base64_chars.find(c);
                return pos != std::string::npos
                           ? static_cast<unsigned char>(pos)
                           : 0;
        };

        size_t in_len = encoded_string.size();
        size_t i = 0;
        size_t in_ = 0;
        unsigned char char_array_4[4], char_array_3[3];

        // Early check to prevent any overflow issues
        if (in_len > MAX_ENCODED_SIZE) {
                throw std::runtime_error(
                    "Base64 input too large: exceeds reasonable limit");
        }

        // Rough estimate of decoded size
        size_t decoded_size = (in_len * 3) / 4;
        if (decoded_size > MAX_DECODED_SIZE)
                throw std::runtime_error(
                    "Base64 input too large: exceeds 100 MB limit");

        std::vector<unsigned char> decoded_data;
        try {
                decoded_data.reserve(decoded_size);
        } catch (const std::bad_alloc &) {
                throw std::runtime_error(
                    "Cannot allocate memory for base64 decoding");
        }

        while (in_len-- && encoded_string[in_] != '=' &&
               is_base64(encoded_string[in_])) {
                char_array_4[i++] = encoded_string[in_++];
                if (i == 4) {
                        for (i = 0; i < 4; i++)
                                char_array_4[i] = base64_index(char_array_4[i]);

                        char_array_3[0] = (char_array_4[0] << 2) +
                                          ((char_array_4[1] & 0x30) >> 4);
                        char_array_3[1] = ((char_array_4[1] & 0xf) << 4) +
                                          ((char_array_4[2] & 0x3c) >> 2);
                        char_array_3[2] =
                            ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

                        if (decoded_data.size() + 3 > MAX_DECODED_SIZE) {
                                throw std::runtime_error(
                                    "Decoded data exceeds size limit during "
                                    "processing");
                        }

                        for (i = 0; i < 3; i++)
                                decoded_data.push_back(char_array_3[i]);

                        i = 0;
                }
        }

        // Process remaining characters
        if (i > 0) {
                for (size_t j = i; j < 4; j++)
                        char_array_4[j] = 0;

                for (size_t j = 0; j < 4; j++)
                        char_array_4[j] = is_base64(char_array_4[j])
                                              ? base64_index(char_array_4[j])
                                              : 0;

                char_array_3[0] =
                    (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
                char_array_3[1] = ((char_array_4[1] & 0xf) << 4) +
                                  ((char_array_4[2] & 0x3c) >> 2);
                char_array_3[2] =
                    ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

                if (i > 1) {
                        size_t remaining_bytes = i - 1;
                        if (decoded_data.size() + remaining_bytes >
                            MAX_DECODED_SIZE) {
                                throw std::runtime_error(
                                    "Decoded data exceeds size limit during "
                                    "final processing");
                        }

                        for (size_t j = 0; j < i - 1; j++)
                                decoded_data.push_back(char_array_3[j]);
                }
        }

        return decoded_data;
}

TagLib::StringList
getOggFieldListCaseInsensitive(const TagLib::Ogg::XiphComment *comment,
                               const std::string &fieldName)
{
        if (!comment) {
                return TagLib::StringList();
        }

        // Use TagLib::String for safer Unicode handling
        TagLib::String targetField(fieldName, TagLib::String::UTF8);
        TagLib::String lowerTargetField =
            targetField.upper(); // TagLib handles case conversion safely

        TagLib::Ogg::FieldListMap fieldListMap = comment->fieldListMap();

        for (auto it = fieldListMap.begin(); it != fieldListMap.end(); ++it) {
                TagLib::String currentKey = it->first;

                if (currentKey.upper() == lowerTargetField) {
                        return it->second;
                }
        }

        return TagLib::StringList();
}

extern "C" {
#include "utils/utils.h"

// Function to read a 32-bit unsigned integer from buffer in big-endian
// format
unsigned int read_uint32_be(const unsigned char *buffer,
                            size_t buffer_size, size_t offset)
{
        if (buffer == nullptr || offset + 4 > buffer_size) {
                // Handle error - throw exception, return error code,
                // etc.
                throw std::runtime_error(
                    "Buffer overflow in read_uint32_be");
        }

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
        size_t dataSize = data.size();

        auto readUInt32 = [&](uint32_t &value) -> bool {
                if (offset + 4 > dataSize)
                        return false;
                value = (ptr[offset] << 24) | (ptr[offset + 1] << 16) |
                        (ptr[offset + 2] << 8) | ptr[offset + 3];
                offset += 4;
                return true;
        };

        auto safeAdd = [](size_t a, uint32_t b) -> bool {
                return b <=
                       SIZE_MAX - a; // Check if a + b would overflow
        };

        uint32_t pictureType, mimeLength, descLength, width, height,
            depth, colors, dataLength;

        if (!readUInt32(pictureType) || !readUInt32(mimeLength))
                return;

        if (mimeLength > MAX_REASONABLE_SIZE ||
            offset + mimeLength > dataSize)
                return;

        // Check for overflow before adding
        if (!safeAdd(offset, mimeLength) ||
            offset + mimeLength > dataSize)
                return;

        mimeType = std::string(
            reinterpret_cast<const char *>(&ptr[offset]), mimeLength);
        offset += mimeLength;

        if (!readUInt32(descLength))
                return;

        if (!safeAdd(offset, descLength) ||
            offset + descLength > dataSize)
                return;

        offset += descLength;

        if (!readUInt32(width) || !readUInt32(height) ||
            !readUInt32(depth) || !readUInt32(colors) ||
            !readUInt32(dataLength))
                return;

        if (!safeAdd(offset, dataLength) ||
            offset + dataLength > dataSize)
                return;

        imageData.assign(&ptr[offset], &ptr[offset + dataLength]);
}

#if HAVE_COMPLEXPROPERTIES

bool extractCoverArtFromOgg(const std::string &audioFilePath,
                            const std::string &outputFileName)
{
        TagLib::FileRef f(audioFilePath.c_str(), true);
        if (!f.file() || !f.file()->isOpen()) {
                std::cerr
                    << "Error: Could not open file: " << audioFilePath
                    << std::endl;
                return false;
        }

        auto pictures = f.file()->complexProperties("PICTURE");
        if (pictures.isEmpty()) {
                std::cerr << "No cover art found in the file (via "
                             "complexProperties).\n";
                return false;
        }

        // Use the first picture found (usually the front cover)
        for (const auto &pic : pictures) {
                auto it_data = pic.find("data");

                if (it_data == pic.end())
                        continue; // Skip if no data

                // Write the image data to a file
                std::ofstream outFile(outputFileName, std::ios::binary);
                if (!outFile) {
                        std::cerr << "Error: Could not write to output "
                                     "file.\n";
                        return false;
                }
                TagLib::ByteVector bv = it_data->second.toByteVector();
                outFile.write(bv.data(), bv.size());
                outFile.close();

                return true;
        }

        std::cerr
            << "No usable cover image found in complexProperties().\n";
        return false;
}

#else

bool extractCoverArtFromOgg(const std::string &audioFilePath,
                            const std::string &outputFileName)
{
        TagLib::File *file = nullptr;
        TagLib::Tag *tag = nullptr;

        // Try to open as Ogg Vorbis
        file = new TagLib::Vorbis::File(audioFilePath.c_str());
        if (!file->isValid()) {
                delete file;
                // Try to open as Opus
                file =
                    new TagLib::Ogg::Opus::File(audioFilePath.c_str());
                if (!file->isValid()) {
                        delete file;
                        std::cerr << "Error: File not found or not a "
                                     "valid Ogg Vorbis or Opus file."
                                  << std::endl;
                        return false; // File not found or invalid
                }
        }

        tag = file->tag();

        const TagLib::Ogg::XiphComment *xiphComment =
            dynamic_cast<TagLib::Ogg::XiphComment *>(tag);
        if (!xiphComment) {
                std::cerr << "Error: No XiphComment found in the file."
                          << std::endl;
                delete file;
                return false; // No cover art found
        }

        // Check METADATA_BLOCK_PICTURE
        TagLib::StringList pictureList = getOggFieldListCaseInsensitive(
            xiphComment, "METADATA_BLOCK_PICTURE");

        if (!pictureList.isEmpty()) {
                std::string base64Data =
                    pictureList.front().to8Bit(true);
                std::vector<unsigned char> decodedData =
                    decodeBase64(base64Data);

                std::string mimeType;
                std::vector<unsigned char> imageData;
                parseFlacPictureBlock(decodedData, mimeType, imageData);

                std::ofstream outFile(outputFileName, std::ios::binary);
                if (!outFile) {
                        std::cerr
                            << "Error: Could not write to output file."
                            << std::endl;
                        delete file;
                        return false; // Could not write to output file
                }
                outFile.write(
                    reinterpret_cast<const char *>(imageData.data()),
                    imageData.size());

                outFile.close();
                delete file;
                return true; // Success
        }

        // Check COVERART and COVERARTMIME
        TagLib::StringList coverArtList =
            getOggFieldListCaseInsensitive(xiphComment, "COVERART");
        TagLib::StringList coverArtMimeList =
            getOggFieldListCaseInsensitive(xiphComment, "COVERARTMIME");
        if (!coverArtList.isEmpty() && !coverArtMimeList.isEmpty()) {
                std::string base64Data =
                    coverArtList.front().to8Bit(true);
                std::vector<unsigned char> imageData =
                    decodeBase64(base64Data);

                std::ofstream outFile(outputFileName, std::ios::binary);
                if (!outFile) {
                        std::cerr
                            << "Error: Could not write to output file."
                            << std::endl;
                        delete file;
                        return false; // Could not write to output file
                }
                outFile.write(
                    reinterpret_cast<const char *>(imageData.data()),
                    imageData.size());
                outFile.close();
                delete file;
                return true; // Success
        }

        std::cerr << "No cover art found in the file." << std::endl;
        delete file;
        return false; // No cover art found
}

#endif

bool looksLikeJpeg(const std::vector<unsigned char> &data)
{
        return data.size() > 4 && data[0] == 0xFF && data[1] == 0xD8 &&
               data[data.size() - 2] == 0xFF &&
               data[data.size() - 1] == 0xD9;
}
bool looksLikePng(const std::vector<unsigned char> &data)
{
        static const unsigned char pngHeader[8] = {
            0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
        if (data.size() < 12)
                return false;
        if (memcmp(data.data(), pngHeader, 8) != 0)
                return false;
        // Look for IEND within last 16 bytes
        for (size_t i = data.size() >= 16 ? data.size() - 16 : 0;
             i + 3 < data.size(); ++i) {
                if (memcmp(&data[i], "IEND", 4) == 0)
                        return true;
        }
        return false;
}
bool looksLikeWebp(const std::vector<unsigned char> &data)
{
        return data.size() > 12 && memcmp(&data[0], "RIFF", 4) == 0 &&
               memcmp(&data[8], "WEBP", 4) == 0;
}
bool isAudioOggStreamHeader(const ogg_packet &headerPacket)
{
        if (headerPacket.bytes >= 7 && headerPacket.packet[0] == 0x01 &&
            memcmp(headerPacket.packet + 1, "vorbis", 6) == 0)
                return true;
        if (headerPacket.bytes >= 8 &&
            memcmp(headerPacket.packet, "OpusHead", 8) == 0)
                return true;
        if (headerPacket.bytes >= 4 &&
            memcmp(headerPacket.packet, "fLaC", 4) == 0)
                return true;
        return false;
}

bool extractCoverArtFromOggVideo(const std::string &audioFilePath,
                                 const std::string &outputFileName)
{
        FILE *oggFile = fopen(audioFilePath.c_str(), "rb");
        if (!oggFile) {
                std::cerr
                    << "Error: Could not open file: " << audioFilePath
                    << std::endl;
                return false;
        }

        ogg_sync_state oy;
        ogg_sync_init(&oy);

        ogg_page og;
        ogg_packet op;

        std::map<int, ogg_stream_state> streams;
        std::map<int, bool> isHeaderParsed;
        std::map<int, bool> isAudioStream;
        std::map<int, std::vector<unsigned char>> streamPackets;

        char *buffer;
        int bytes = 0;

        while (true) {
                buffer = ogg_sync_buffer(&oy, 4096);
                bytes = fread(buffer, 1, 4096, oggFile);
                ogg_sync_wrote(&oy, bytes);

                while (ogg_sync_pageout(&oy, &og) == 1) {
                        int serialNo = ogg_page_serialno(&og);

                        // Lazily create state for new streams
                        if (streams.find(serialNo) == streams.end()) {
                                ogg_stream_state os;
                                ogg_stream_init(&os, serialNo);
                                streams[serialNo] = os;
                                isHeaderParsed[serialNo] = false;
                                isAudioStream[serialNo] = false;
                        }
                        ogg_stream_state &os = streams[serialNo];
                        ogg_stream_pagein(&os, &og);

                        while (ogg_stream_packetout(&os, &op) == 1) {
                                // Only decide on audio-ness for first
                                // packet
                                if (!isHeaderParsed[serialNo]) {
                                        isAudioStream[serialNo] =
                                            isAudioOggStreamHeader(op);
                                        isHeaderParsed[serialNo] = true;

                                        if (isAudioStream[serialNo])
                                                continue;
                                }
                                if (!isAudioStream[serialNo]) {
                                        // For image/video: collect all
                                        // packets from this stream
                                        streamPackets[serialNo].insert(
                                            streamPackets[serialNo]
                                                .end(),
                                            op.packet,
                                            op.packet + op.bytes);
                                }
                        }
                }
                if (bytes == 0)
                        break;
        }

        // Clean up
        for (auto &entry : streams)
                ogg_stream_clear(&(entry.second));
        ogg_sync_clear(&oy);
        fclose(oggFile);

        // Write out a valid image stream
        for (auto &kv : streamPackets) {
                const auto &data = kv.second;
                if (looksLikeJpeg(data) || looksLikePng(data) ||
                    looksLikeWebp(data)) {
                        std::ofstream outFile(outputFileName,
                                              std::ios::binary);
                        if (!outFile) {
                                std::cerr << "Error: Could not write "
                                             "to output file."
                                          << std::endl;
                                return false;
                        }
                        outFile.write(
                            reinterpret_cast<const char *>(data.data()),
                            data.size());
                        outFile.close();
                        return true;
                }
        }
        std::cerr << "No embedded image stream found in file."
                  << std::endl;
        return false;
}

bool extractCoverArtFromMp3(const std::string &inputFile,
                            const std::string &coverFilePath)
{
        TagLib::MPEG::File file(inputFile.c_str());
        if (!file.isValid()) {
                return false;
        }

        const TagLib::ID3v2::Tag *id3v2tag = file.ID3v2Tag();
        if (id3v2tag) {
                // Collect all attached picture frames
                TagLib::ID3v2::FrameList frames;
                frames.append(id3v2tag->frameListMap()["APIC"]);
                frames.append(id3v2tag->frameListMap()["PIC"]);

                if (!frames.isEmpty()) {
                        for (auto it = frames.begin();
                             it != frames.end(); ++it) {
                                const TagLib::ID3v2::
                                    AttachedPictureFrame *picFrame =
                                        dynamic_cast<
                                            TagLib::ID3v2::
                                                AttachedPictureFrame *>(
                                            *it);
                                if (picFrame) {
                                        // Access picture data and MIME
                                        // type
                                        TagLib::ByteVector pictureData =
                                            picFrame->picture();
                                        TagLib::String mimeType =
                                            picFrame->mimeType();

                                        // Construct the output file
                                        // path
                                        std::string outputFilePath =
                                            coverFilePath;

                                        // Write the image data to a
                                        // file
                                        FILE *outFile = fopen(
                                            outputFilePath.c_str(),
                                            "wb");
                                        if (outFile) {
                                                fwrite(
                                                    pictureData.data(),
                                                    1,
                                                    pictureData.size(),
                                                    outFile);
                                                fclose(outFile);

                                                return true;
                                        } else {
                                                return false; // Failed
                                                              // to open
                                                              // output
                                                              // file
                                        }

                                        // Break if only the first image
                                        // is needed
                                        break;
                                }
                        }
                } else {
                        return false; // No picture frames found
                }
        } else {
                return false; // No ID3v2 tag found
        }

        return true; // Success
}

bool extractCoverArtFromFlac(const std::string &inputFile,
                             const std::string &coverFilePath)
{
        TagLib::FLAC::File file(inputFile.c_str());

        if (file.pictureList().size() > 0) {
                const TagLib::FLAC::Picture *picture =
                    file.pictureList().front();
                if (picture) {
                        FILE *coverFile =
                            fopen(coverFilePath.c_str(), "wb");
                        if (coverFile) {
                                fwrite(picture->data().data(), 1,
                                       picture->data().size(),
                                       coverFile);
                                fclose(coverFile);
                                return true;
                        } else {
                                return false;
                        }
                }
        }

        return false;
}

bool extractCoverArtFromWav(const std::string &inputFile,
                            const std::string &coverFilePath)
{
        TagLib::RIFF::WAV::File file(inputFile.c_str());
        if (!file.isValid()) {
                return false;
        }

        const TagLib::ID3v2::Tag *id3v2tag = file.ID3v2Tag();
        if (id3v2tag) {
                // Collect all attached picture frames
                TagLib::ID3v2::FrameList frames;
                frames.append(id3v2tag->frameListMap()["APIC"]);
                frames.append(id3v2tag->frameListMap()["PIC"]);

                if (!frames.isEmpty()) {
                        for (auto it = frames.begin();
                             it != frames.end(); ++it) {
                                const TagLib::ID3v2::
                                    AttachedPictureFrame *picFrame =
                                        dynamic_cast<
                                            TagLib::ID3v2::
                                                AttachedPictureFrame *>(
                                            *it);
                                if (picFrame) {
                                        // Access picture data and MIME
                                        // type
                                        TagLib::ByteVector pictureData =
                                            picFrame->picture();
                                        TagLib::String mimeType =
                                            picFrame->mimeType();

                                        // Construct the output file
                                        // path
                                        std::string outputFilePath =
                                            coverFilePath;

                                        // Write the image data to a
                                        // file
                                        FILE *outFile = fopen(
                                            outputFilePath.c_str(),
                                            "wb");
                                        if (outFile) {
                                                fwrite(
                                                    pictureData.data(),
                                                    1,
                                                    pictureData.size(),
                                                    outFile);
                                                fclose(outFile);

                                                return true;
                                        } else {
                                                return false; // Failed
                                                              // to open
                                                              // output
                                                              // file
                                        }

                                        // Break if only the first image
                                        // is needed
                                        break;
                                }
                        }
                } else {
                        return false; // No picture frames found
                }
        } else {
                return false; // No ID3v2 tag found
        }

        return true; // Success
}

bool extractCoverArtFromOpus(const std::string &audioFilePath,
                             const std::string &outputFileName)
{
        int error;
        OggOpusFile *of = op_open_file(audioFilePath.c_str(), &error);
        if (error != OPUS_OK || of == nullptr) {
                std::cerr << "Error: Failed to open Opus file."
                          << std::endl;
                return false;
        }

        const OpusTags *tags = op_tags(of, -1);
        if (!tags) {
                std::cerr << "Error: No tags found in Opus file."
                          << std::endl;
                op_free(of);
                return false;
        }

        // Search through the metadata for an attached picture (if
        // present)
        for (int i = 0; i < tags->comments; ++i) {
                // Check for METADATA_BLOCK_PICTURE
                const char *comment = tags->user_comments[i];
                if (strncasecmp(comment,
                                "METADATA_BLOCK_PICTURE=", 23) == 0) {
                        // Extract the value after
                        // "METADATA_BLOCK_PICTURE="
                        std::string metadataBlockPicture(comment + 23);

                        // Base64-decode this value to get the binary
                        // PICTURE block
                        std::vector<unsigned char> pictureBlock =
                            decodeBase64(metadataBlockPicture);

                        if (pictureBlock.empty()) {
                                std::cerr
                                    << "Failed to decode Base64 data."
                                    << std::endl;
                                op_free(of);
                                return false;
                        }

                        // Now parse the binary pictureBlock to extract
                        // the image data
                        size_t offset = 0;

                        if (pictureBlock.size() < 32) {
                                std::cerr << "Picture block too small."
                                          << std::endl;
                                op_free(of);
                                return false;
                        }

                        // Read PICTURE TYPE
                        read_uint32_be(pictureBlock.data(),
                                       pictureBlock.size(), offset);
                        offset += 4;

                        // Read MIME TYPE LENGTH
                        unsigned int mimeTypeLength =
                            read_uint32_be(pictureBlock.data(),
                                           pictureBlock.size(), offset);
                        offset += 4;

                        // Read MIME TYPE
                        if (offset + mimeTypeLength >
                            pictureBlock.size()) {
                                op_free(of);
                                return false;
                        }
                        offset += mimeTypeLength;

                        // Read DESCRIPTION LENGTH
                        unsigned int descriptionLength =
                            read_uint32_be(pictureBlock.data(),
                                           pictureBlock.size(), offset);
                        offset += 4;

                        // Read DESCRIPTION
                        if (offset + descriptionLength >
                            pictureBlock.size()) {
                                op_free(of);
                                return false;
                        }
                        offset += descriptionLength;
                        // Optionally print or ignore description

                        // Read WIDTH
                        read_uint32_be(pictureBlock.data(),
                                       pictureBlock.size(), offset);
                        offset += 4;

                        // Read HEIGHT
                        read_uint32_be(pictureBlock.data(),
                                       pictureBlock.size(), offset);
                        offset += 4;
                        ;

                        // Read COLOR DEPTH
                        read_uint32_be(pictureBlock.data(),
                                       pictureBlock.size(), offset);
                        offset += 4;

                        // Read NUMBER OF COLORS
                        read_uint32_be(pictureBlock.data(),
                                       pictureBlock.size(), offset);
                        offset += 4;

                        // Read DATA LENGTH
                        unsigned int dataLength =
                            read_uint32_be(pictureBlock.data(),
                                           pictureBlock.size(), offset);
                        offset += 4;

                        if (offset + dataLength > pictureBlock.size()) {
                                std::cerr
                                    << "Invalid image data length."
                                    << std::endl;
                                op_free(of);
                                return false;
                        }

                        // Extract image data
                        std::vector<unsigned char> imageData(
                            pictureBlock.begin() + offset,
                            pictureBlock.begin() + offset + dataLength);

                        // Save image data to file
                        std::ofstream outFile(outputFileName,
                                              std::ios::binary);
                        if (!outFile) {
                                std::cerr << "Error: Could not write "
                                             "to output file."
                                          << std::endl;
                                op_free(of);
                                return false;
                        }
                        outFile.write(reinterpret_cast<const char *>(
                                          imageData.data()),
                                      imageData.size());
                        outFile.close();

                        op_free(of);
                        return true;
                }
        }

        std::cerr << "No cover art found in the metadata." << std::endl;
        op_free(of);
        return false;
}

bool extractCoverArtFromMp4(const std::string &inputFile,
                            const std::string &coverFilePath)
{
        TagLib::MP4::File file(inputFile.c_str());

        if (!file.isValid()) {
                return false;
        }

        const TagLib::MP4::Item coverItem = file.tag()->item("covr");

        if (coverItem.isValid()) {
                TagLib::MP4::CoverArtList coverArtList =
                    coverItem.toCoverArtList();
                if (!coverArtList.isEmpty()) {
                        const TagLib::MP4::CoverArt &coverArt =
                            coverArtList.front();
                        FILE *coverFile =
                            fopen(coverFilePath.c_str(), "wb");
                        if (coverFile) {
                                fwrite(coverArt.data().data(), 1,
                                       coverArt.data().size(),
                                       coverFile);
                                fclose(coverFile);
                                return true; // Success
                        } else {
                                fprintf(
                                    stderr,
                                    "Could not open output file '%s'\n",
                                    coverFilePath.c_str());
                                return false; // Failed to open the
                                              // output file
                        }
                }
        }

        return false; // No valid cover item or cover art found
}

void trimcpp(std::string &str)
{
        // Remove leading spaces
        str.erase(str.begin(),
                  std::find_if(str.begin(), str.end(),
                               [](unsigned char ch) { return !std::isspace(ch); }));

        // Remove trailing spaces
        str.erase(std::find_if(str.rbegin(), str.rend(),
                               [](unsigned char ch) { return !std::isspace(ch); })
                      .base(),
                  str.end());
}

void turnFilePathIntoTitle(const char *filePath, char *title,
                           size_t titleMaxLength)
{
        std::string filePathStr(
            filePath); // Convert the C-style string to std::string

        size_t lastSlashPos = filePathStr.find_last_of(
            "/\\"); // Find the last '/' or '\\'
        size_t lastDotPos =
            filePathStr.find_last_of('.'); // Find the last '.'

        // Validate that both positions exist and the dot is after the
        // last slash
        if (lastSlashPos != std::string::npos &&
            lastDotPos != std::string::npos &&
            lastDotPos > lastSlashPos) {
                // Extract the substring between the last slash and the
                // last dot
                std::string extractedTitle = filePathStr.substr(
                    lastSlashPos + 1, lastDotPos - lastSlashPos - 1);

                // Trim any unwanted spaces
                trimcpp(extractedTitle);

                // Ensure title is not longer than titleMaxLength,
                // including the null terminator
                if (extractedTitle.length() >= titleMaxLength) {
                        extractedTitle = extractedTitle.substr(
                            0, titleMaxLength - 1);
                }

                // Copy the result into the output char* title, ensuring
                // no overflow
                c_strcpy(
                    title, extractedTitle.c_str(),
                    titleMaxLength -
                        1); // Copy up to titleMaxLength - 1 characters
                title[titleMaxLength - 1] =
                    '\0'; // Null-terminate the string
        } else {
                // If no valid title is found, ensure title is an empty
                // string
                title[0] = '\0';
        }
}

double parseDecibelValue(const TagLib::String &dbString)
{
        double val = 0.0;
        try {
                std::string valStr = dbString.to8Bit(true);
                std::string filtered;
                for (char c : valStr) {
                        if (std::isdigit((unsigned char)c) ||
                            c == '.' || c == '-' || c == '+' ||
                            c == 'e' || c == 'E') {
                                filtered.push_back(c);
                        }
                }
                val = std::stod(filtered);
        } catch (...) {
        }
        return val;
}

static bool detectLrcFormat(const TagLib::StringList &lines)
{
        for (const auto &line : lines) {
                std::string text = line.toCString(true);
                if (text.size() > 3 && text[0] == '[' && std::isdigit((unsigned char)text[1]))
                        return true;
        }
        return false;
}

static bool parseTimedLyricsFromTagLines(const TagLib::StringList &lines, Lyrics *lyrics)
{
        size_t capacity = 64;
        lyrics->lines = (LyricsLine *)malloc(sizeof(LyricsLine) * capacity);
        if (!lyrics->lines)
                return false;

        for (const auto &line : lines) {
                std::string text = line.toCString(true);
                if (text.empty() || text[0] != '[')
                        continue;

                int min = 0, sec = 0, cs = 0;
                char lyricText[512] = {0};

                if (sscanf(text.c_str(), "[%d:%d.%d]%511[^\r\n]", &min, &sec, &cs, lyricText) == 4) {
                        if (lyrics->count == capacity) {
                                capacity *= 2;
                                LyricsLine *newLines = (LyricsLine *)realloc(lyrics->lines, sizeof(LyricsLine) * capacity);
                                if (!newLines)
                                        return false;
                                lyrics->lines = newLines;
                        }

                        char *start = lyricText;
                        while (isspace((unsigned char)*start))
                                start++;
                        char *end = start + strlen(start);
                        while (end > start && isspace((unsigned char)*(end - 1)))
                                *(--end) = '\0';

                        lyrics->lines[lyrics->count].timestamp = min * 60.0 + sec + cs / 100.0;
                        lyrics->lines[lyrics->count].text = strdup(start);
                        if (!lyrics->lines[lyrics->count].text)
                                return false;

                        lyrics->count++;
                }
        }

        lyrics->isTimed = 1;
        return (lyrics->count > 0);
}

static bool parseUntimedLyricsFromTagLines(const TagLib::StringList &lines, Lyrics *lyrics)
{
        size_t capacity = lines.size() > 64 ? lines.size() : 64;
        lyrics->lines = (LyricsLine *)malloc(sizeof(LyricsLine) * capacity);
        if (!lyrics->lines)
                return false;

        lyrics->count = 0;

        for (size_t i = 0; i < lines.size(); ++i) {
                std::string text = lines[i].toCString(true);

                // Trim leading/trailing whitespace
                size_t start = 0;
                while (start < text.size() && std::isspace((unsigned char)text[start]))
                        start++;
                size_t end = text.size();
                while (end > start && std::isspace((unsigned char)text[end - 1]))
                        end--;
                text = text.substr(start, end - start);

                if (text.empty())
                        continue;

                if (lyrics->count == capacity) {
                        capacity *= 2;
                        LyricsLine *newLines = (LyricsLine *)realloc(lyrics->lines, sizeof(LyricsLine) * capacity);
                        if (!newLines)
                                return false;
                        lyrics->lines = newLines;
                }

                lyrics->lines[lyrics->count].timestamp = 0.0;
                lyrics->lines[lyrics->count].text = strdup(text.c_str());
                if (!lyrics->lines[lyrics->count].text)
                        return false;

                lyrics->count++;
        }

        lyrics->isTimed = 0;
        return (lyrics->count > 0);
}

static bool loadLyricsVorbisFromTag(TagLib::Ogg::XiphComment *tag, Lyrics **lyricsOut)
{
    if (!tag || !lyricsOut)
        return false;

    auto fields = tag->fieldListMap();

    const char *keys[] = {"LYRICS", "UNSYNCEDLYRICS", "lyrics"};
    const TagLib::StringList *entry = nullptr;

    for (auto key : keys)
    {
        if (fields.contains(key))
        {
            entry = &fields[key];
            break;
        }
    }

    if (!entry || entry->isEmpty())
        return false;

    // Build final line list
    TagLib::StringList lines;

    for (const auto &val : *entry)
    {
        TagLib::StringList split = val.split("\n");
        for (const auto &ln : split)
            lines.append(ln);
    }

    // Remove trailing blanks
    while (!lines.isEmpty() && lines.back().stripWhiteSpace().isEmpty())
    {
        auto it = lines.end();
        --it;
        lines.erase(it);
    }

    if (lines.isEmpty())
        return false;

    Lyrics *lyrics = (Lyrics *)calloc(1, sizeof(Lyrics));
    if (!lyrics)
        return false;

    lyrics->max_length = 1024;

    bool looksLikeLrc = detectLrcFormat(lines);
    bool ok = looksLikeLrc ? parseTimedLyricsFromTagLines(lines, lyrics)
                           : parseUntimedLyricsFromTagLines(lines, lyrics);

    if (!ok)
    {
        freeLyrics(lyrics);
        return false;
    }

    *lyricsOut = lyrics;
    return true;
}

static bool loadLyricsVorbisFLAC(TagLib::FLAC::File *file, Lyrics **lyricsOut)
{
        return loadLyricsVorbisFromTag(file->xiphComment(), lyricsOut);
}

static bool loadLyricsVorbisOgg(TagLib::Ogg::Vorbis::File *file, Lyrics **lyricsOut)
{
        return loadLyricsVorbisFromTag(file->tag(), lyricsOut);
}

static bool loadLyricsVorbisOpus(TagLib::Ogg::Opus::File *file, Lyrics **lyricsOut)
{
        return loadLyricsVorbisFromTag(file->tag(), lyricsOut);
}

static bool loadLyricsFromSYLTTag(TagLib::ID3v2::Tag *id3v2Tag, Lyrics **lyricsOut)
{
        if (!id3v2Tag || !lyricsOut)
                return false;

        auto frames = id3v2Tag->frameList("SYLT");
        if (frames.isEmpty())
                return false;

        // Count total lines across all SYLT frames
        size_t totalLines = 0;
        for (auto frame : frames) {
                auto sylt = dynamic_cast<TagLib::ID3v2::SynchronizedLyricsFrame *>(frame);
                if (!sylt)
                        continue;
                totalLines += sylt->synchedText().size();
        }

        if (totalLines == 0)
                return false;

        Lyrics *lyrics = (Lyrics *)calloc(1, sizeof(Lyrics));
        if (!lyrics)
                return false;

        lyrics->max_length = 1024;
        lyrics->lines = (LyricsLine *)malloc(sizeof(LyricsLine) * totalLines);
        if (!lyrics->lines) {
                free(lyrics);
                return false;
        }

        lyrics->count = 0;
        for (auto frame : frames) {
                auto sylt = dynamic_cast<TagLib::ID3v2::SynchronizedLyricsFrame *>(frame);
                if (!sylt)
                        continue;

                for (auto lyric : sylt->synchedText()) {
                        char *text = strdup(lyric.text.toCString(true));
                        if (!text)
                                continue;

                        // Trim leading/trailing whitespace
                        char *start = text;
                        while (isspace((unsigned char)*start))
                                start++;
                        char *end = start + strlen(start);
                        while (end > start && isspace((unsigned char)*(end - 1)))
                                *(--end) = '\0';

                        lyrics->lines[lyrics->count].timestamp = lyric.time / 1000.0; // ms → s
                        lyrics->lines[lyrics->count].text = strdup(start);
                        free(text);

                        if (!lyrics->lines[lyrics->count].text) {
                                freeLyrics(lyrics);
                                return false;
                        }

                        lyrics->count++;
                }
        }

        lyrics->isTimed = 1;
        *lyricsOut = lyrics;
        return true;
}

int extractTags(const char *input_file, TagSettings *tag_settings,
                double *duration, const char *coverFilePath, Lyrics **lyrics)
{
        memset(tag_settings, 0,
               sizeof(TagSettings)); // Initialize tag settings

        tag_settings->replaygainTrack = 0.0;
        tag_settings->replaygainAlbum = 0.0;

        // Use TagLib's FileRef for generic file parsing.
        TagLib::FileRef f(input_file);
        if (f.isNull() || !f.file()) {
                fprintf(stderr,
                        "FileRef is null or file could not be opened: "
                        "'%s'\n",
                        input_file);

                char title[4096];
                turnFilePathIntoTitle(input_file, title, 4096);
                c_strcpy(tag_settings->title, title,
                         sizeof(tag_settings->title) - 1);
                tag_settings->title[sizeof(tag_settings->title) - 1] =
                    '\0';

                return -1;
        }

        // Extract tags using the stable method that worked before.
        const TagLib::Tag *tag = f.tag();
        if (!tag) {
                fprintf(stderr, "Tag is null for file '%s'\n",
                        input_file);
                return -2;
        }

        // Copy the title
        c_strcpy(tag_settings->title, tag->title().toCString(true),
                 sizeof(tag_settings->title) - 1);
        tag_settings->title[sizeof(tag_settings->title) - 1] = '\0';

        // Check if the title is empty, and if so, use the file path to
        // generate a title
        if (strnlen(tag_settings->title, 10) == 0) {
                char title[4096];
                turnFilePathIntoTitle(input_file, title, 4096);
                c_strcpy(tag_settings->title, title,
                         sizeof(tag_settings->title) - 1);
                tag_settings->title[sizeof(tag_settings->title) - 1] =
                    '\0';
        } else {
                // Copy the artist
                c_strcpy(tag_settings->artist,
                         tag->artist().toCString(true),
                         sizeof(tag_settings->artist) - 1);
                tag_settings->artist[sizeof(tag_settings->artist) - 1] =
                    '\0';

                // Copy the album
                c_strcpy(tag_settings->album,
                         tag->album().toCString(true),
                         sizeof(tag_settings->album) - 1);
                tag_settings->album[sizeof(tag_settings->album) - 1] =
                    '\0';

                // Copy the year as date
                snprintf(tag_settings->date, sizeof(tag_settings->date),
                         "%d", (int)tag->year());

                if (tag_settings->date[0] == '0') {
                        tag_settings->date[0] = '\0';
                }
        }

        if (*lyrics == nullptr) {
                if (auto mpegFile = dynamic_cast<TagLib::MPEG::File *>(f.file()))
                        loadLyricsFromSYLTTag(mpegFile->ID3v2Tag(), lyrics);
                if (auto flacFile = dynamic_cast<TagLib::FLAC::File *>(f.file()))
                        loadLyricsVorbisFLAC(flacFile, lyrics);
                else if (auto oggFile = dynamic_cast<TagLib::Ogg::Vorbis::File *>(f.file()))
                        loadLyricsVorbisOgg(oggFile, lyrics);
                else if (auto opusFile = dynamic_cast<TagLib::Ogg::Opus::File *>(f.file()))
                        loadLyricsVorbisOpus(opusFile, lyrics);
        }

        // Extract audio properties for duration.
        if (f.audioProperties()) {
                *duration = f.audioProperties()->lengthInSeconds();
        } else {
                *duration = 0.0;
                fprintf(stderr,
                        "No audio properties found for file '%s'\n",
                        input_file);
                return -2;
        }

        // Extract replay gain information
        if (std::string(input_file).find(".mp3") != std::string::npos) {
                TagLib::MPEG::File mp3File(input_file);
                TagLib::ID3v2::Tag *id3v2Tag = mp3File.ID3v2Tag();

                if (id3v2Tag) {
                        // Retrieve all TXXX frames
                        TagLib::ID3v2::FrameList frames =
                            id3v2Tag->frameList("TXXX");
                        for (TagLib::ID3v2::FrameList::Iterator it =
                                 frames.begin();
                             it != frames.end(); ++it) {
                                // Cast to the user-text (TXXX) frame
                                // class
                                TagLib::ID3v2::TextIdentificationFrame
                                    *txxx = dynamic_cast<
                                        TagLib::ID3v2::
                                            TextIdentificationFrame *>(
                                        *it);
                                if (!txxx)
                                        continue;

                                TagLib::StringList fields =
                                    txxx->fieldList();
                                if (fields.size() >= 2) {
                                        TagLib::String desc = fields[0];
                                        TagLib::String val = fields[1];

                                        if (desc.upper() ==
                                            "REPLAYGAIN_TRACK_GAIN") {
                                                tag_settings
                                                    ->replaygainTrack =
                                                    parseDecibelValue(
                                                        val);
                                        } else if (desc.upper() ==
                                                   "REPLAYGAIN_ALBUM_"
                                                   "GAIN") {
                                                tag_settings
                                                    ->replaygainAlbum =
                                                    parseDecibelValue(
                                                        val);
                                        }
                                }
                        }
                }

                TagLib::APE::Tag *apeTag = mp3File.APETag();

                if (apeTag) {
                        TagLib::APE::ItemListMap items =
                            apeTag->itemListMap();
                        for (auto it = items.begin(); it != items.end();
                             ++it) {
                                std::string key =
                                    it->first.upper().toCString();
                                TagLib::String value =
                                    it->second.toString();

                                if (key == "REPLAYGAIN_TRACK_GAIN") {
                                        tag_settings->replaygainTrack =
                                            parseDecibelValue(value);
                                } else if (key == "REPLAYGAIN_ALBUM_GAIN") {
                                        tag_settings->replaygainAlbum =
                                            parseDecibelValue(value);
                                }
                        }
                }
        } else if (std::string(input_file).find(".flac") !=
                   std::string::npos) {
                TagLib::FLAC::File flacFile(input_file);
                TagLib::Ogg::XiphComment *xiphComment =
                    flacFile.xiphComment();

                if (xiphComment) {
                        const TagLib::Ogg::FieldListMap &fieldMap =
                            xiphComment->fieldListMap();

                        auto trackGainIt =
                            fieldMap.find("REPLAYGAIN_TRACK_GAIN");
                        if (trackGainIt != fieldMap.end()) {
                                const TagLib::StringList &
                                    trackGainList = trackGainIt->second;
                                if (!trackGainList.isEmpty()) {
                                        tag_settings->replaygainTrack =
                                            parseDecibelValue(
                                                trackGainList.front());
                                }
                        }

                        auto albumGainIt =
                            fieldMap.find("REPLAYGAIN_ALBUM_GAIN");
                        if (albumGainIt != fieldMap.end()) {
                                const TagLib::StringList &
                                    albumGainList = albumGainIt->second;
                                if (!albumGainList.isEmpty()) {
                                        tag_settings->replaygainAlbum =
                                            parseDecibelValue(
                                                albumGainList.front());
                                }
                        }
                }
        }

        std::string filename(input_file);
        std::string extension =
            filename.substr(filename.find_last_of('.') + 1);
        bool coverArtExtracted = false;

        if (extension == "mp3") {
                coverArtExtracted =
                    extractCoverArtFromMp3(input_file, coverFilePath);
        } else if (extension == "flac") {
                coverArtExtracted =
                    extractCoverArtFromFlac(input_file, coverFilePath);
        } else if (extension == "m4a" || extension == "aac") {
                coverArtExtracted =
                    extractCoverArtFromMp4(input_file, coverFilePath);
        }
        if (extension == "opus") {
                coverArtExtracted =
                    extractCoverArtFromOpus(input_file, coverFilePath);
        } else if (extension == "ogg") {
                coverArtExtracted =
                    extractCoverArtFromOgg(input_file, coverFilePath);

                if (!coverArtExtracted) {
                        coverArtExtracted = extractCoverArtFromOggVideo(
                            input_file, coverFilePath);
                }
        } else if (extension == "wav") {
                coverArtExtracted =
                    extractCoverArtFromWav(input_file, coverFilePath);
        }

        if (coverArtExtracted) {
                return 0;
        } else {
                return -1;
        }
}
}
