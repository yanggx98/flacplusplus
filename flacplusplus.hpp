//----------------------------------------------------------------------------------------------------------------------
//
// flac++ v1.0
// https://github.com/kougyue/flacplusplus
// SPDX-License-Identifier: MIT
//
//----------------------------------------------------------------------------------------------------------------------

#ifndef FLACPLUSPLUS_H
#define FLACPLUSPLUS_H

#include <string>
#include <iostream>
#include <fstream>
#include <map>
#include <vector>
#include <bitset>
#include <malloc.h>

namespace flacpp
{

#define FLAC_LENGHT 4
    ///////////////////////////////
    // meta_block types
    ///////////////////////////////

#define META_TYPE_STREAM_INFO 0
#define META_TYPE_PADDING 1
#define META_TYPE_APPLICATION 2
#define META_TYPE_SEEK_TABLE 3
#define META_TYPE_VORBIS_COMMENT 4
#define META_TYPE_CUESHEET 5
#define META_TYPE_PICTURE 6

    //////////////////////////////
    // parser result
    //////////////////////////////

#define OPEN_FILE_FAILED 1
#define NOT_FLAC_FAILED 2
#define PARSE_FAILED -1
#define PARSE_OK 0

    //////////////////////////////
    // struct
    //////////////////////////////
    //////////////////////////////
    // sub type
    //////////////////////////////
    struct TrackIndex
    {
        long sampleOffset;
        char indexPointNumber;
        char reserved[3];
    };
    struct CuesheetTrack
    {
        long trackOffset;
        char trackNumber;
        char isrc[12];
        // The track type: 0 for audio, 1 for non-audio. This corresponds to the CD-DA Q-channel control bit 3.
        bool trackType;
        // 0 for no pre-emphasis, 1 for pre-emphasis. This corresponds to the CD-DA Q-channel control bit 5;
        bool flag;
        char reserved[14];
        char indexPointCount;
        std::vector<TrackIndex> *trackIndexs;
        CuesheetTrack()
        {
            trackIndexs = new std::vector<TrackIndex>();
        }
        ~CuesheetTrack()
        {
            delete trackIndexs;
            trackIndexs = nullptr;
        }
    };

    struct SeekPoint
    {
        std::string sampleNumber;
        std::string frameOffset;
        short targetFrameSampleCount;
    };

    typedef int BlockType;

    struct MetaBlock
    {
        bool isFinal;
        BlockType blockType;
        int blockSize;
    };

    struct StreamInfo : MetaBlock
    {
        short minimunBlockSize;
        short maximumBlockSize;
        int minimumFrameSize;
        int maximumFrameSize;
        int sampleRate;
        short numberOfChannels; // (number of channels)-1
        short bitsPerSample;    // 	(bits per sample)-1
        long totalSampleCount;  // 0 is unknow
        std::string md5Info;
    };

    struct Padding : MetaBlock
    {
        std::string data;
    };

    struct Application : MetaBlock
    {
        std::string applicationId;
        std::string applicationData;
    };
    struct SeekTable : MetaBlock
    {
        std::vector<SeekPoint> *seekPoints;
        SeekTable()
        {
            seekPoints = new std::vector<SeekPoint>();
        }
        ~SeekTable()
        {
            delete seekPoints;
            seekPoints = nullptr;
        }
    };

    struct VorbisComment : MetaBlock
    {
        std::map<std::string, std::vector<std::string>> *dataMap;
        VorbisComment()
        {
            dataMap = new std::map<std::string, std::vector<std::string>>();
        }
        ~VorbisComment()
        {
            delete dataMap;
            dataMap = nullptr;
        }
    };

    struct Cuesheet : MetaBlock
    {
        char mediaCatalogNumber[128];
        char leadInSamplesCount[8];
        // 1 if the CUESHEET corresponds to a Compact Disc, else 0.
        bool isCompactDisc;
        char reserved[259];
        //	The number of tracks. Must be at least 1 (because of the requisite lead-out track). For CD-DA, this number must be no more than 100 (99 regular tracks and one lead-out track).
        char tracksCount;
        std::vector<CuesheetTrack> *trackIndexs;
        Cuesheet()
        {
            trackIndexs = new std::vector<CuesheetTrack>();
        }
        ~Cuesheet()
        {
            delete trackIndexs;
            trackIndexs = nullptr;
        }
    };

    struct Picture : MetaBlock
    {
        int pictureType;
        int mineTypeLength;
        std::string mineType;
        int descLength;
        std::string desc;
        int pictureWidth;
        int pictureHeight;
        int colorDepth;
        // For indexed-color pictures (e.g. GIF), the number of colors used, or 0 for non-indexed pictures.
        int colorsUseCount;
        int dataLength;
        std::string data;
    };

    //////////////////////////////
    //  classes
    //////////////////////////////

    class FlacInfo
    {
    public:
        StreamInfo *streamInfo = nullptr;
        Padding *padding = nullptr;
        SeekTable *seekTable = nullptr;
        Application *application = nullptr;
        VorbisComment *vorbisComment = nullptr;
        Cuesheet *cuesheet = nullptr;
        Picture *picture = nullptr;
        FlacInfo()
        {
        }
        ~FlacInfo()
        {
            delete streamInfo;
            delete padding;
            delete seekTable;
            delete application;
            delete vorbisComment;
            delete cuesheet;
            delete picture;
            streamInfo = nullptr;
            padding = nullptr;
            seekTable = nullptr;
            application = nullptr;
            vorbisComment = nullptr;
            cuesheet = nullptr;
            picture = nullptr;
        }
    };
    class FlacParser
    {
    public:
        static int parse(std::string fileName, FlacInfo &info)
        {
            std::ifstream is;
            is.open(fileName, std::ios_base::binary);
            if (!is.is_open())
            {
                return OPEN_FILE_FAILED;
            }
            is.seekg(0, std::ios_base::end);
            int length = is.tellg();
            is.seekg(0, std::ios_base::beg);

            char *content = new char[length];
            is.read(content, length);
            is.close();

            char fLaC[FLAC_LENGHT];
            for (int i = 0; i < FLAC_LENGHT; i++)
            {
                fLaC[i] = content[i];
            }
            if (std::string(fLaC).compare("fLaC"))
            {
                return NOT_FLAC_FAILED;
            }

            int offsetAddress = 0;
            content = content + FLAC_LENGHT;
            offsetAddress += FLAC_LENGHT;
            int isFinal = 0;
            do
            {
                int result = parse2(content, info, isFinal);
                if (result == PARSE_FAILED)
                {
                    return PARSE_FAILED;
                }
                // 加上偏移
                content = content + result;
                offsetAddress += result;
            } while (!isFinal);

            content = content - offsetAddress;
            delete content;
            content = nullptr;
            return PARSE_OK;
        }

    private:
        static int parse2(char *data, FlacInfo &info, int &isFinal)
        {
            try
            {
                int index = 0;
                // 是否是最后一个 metablock
                bool isLastMetaBlock = data[index] >> 7;
                int blockType = data[index] & 0x7f;
                // 三个字节拼在一起
                int blockSize = (((data[++index] & 0x00ff) << 16) | ((data[++index] & 0x00ff) << 8)) | (data[++index] & 0x00ff);
                // 这里数到3 下一次循环开始应该是4,所以要+1
                index++;

                // 存放偏移 index 之后的 data，也就是只包含数据块的内容
                char *tempData = data + index;
                char *realData = (char *)malloc(blockSize);
                // 获取 metadata
                for (int i = 0; i < blockSize; i++)
                {
                    realData[i] = tempData[i];
                }
                // 选择合适的解析器
                switch (blockType)
                {
                case META_TYPE_STREAM_INFO:
                    parseStreamInfo(realData, blockSize, info.streamInfo);
                    info.streamInfo->blockSize = blockSize;
                    info.streamInfo->blockType = blockType;
                    info.streamInfo->isFinal = isLastMetaBlock;
                    break;
                case META_TYPE_PICTURE:
                    parsePicture(realData, blockSize, info.picture);
                    info.picture->blockSize = blockSize;
                    info.picture->blockType = blockType;
                    info.picture->isFinal = isLastMetaBlock;
                    break;
                case META_TYPE_SEEK_TABLE:
                    parseSeekTable(realData, blockSize, info.seekTable);
                    info.seekTable->blockSize = blockSize;
                    info.seekTable->blockType = blockType;
                    info.seekTable->isFinal = isLastMetaBlock;
                    break;
                case META_TYPE_VORBIS_COMMENT:
                    parseVorbisComment(realData, blockSize, info.vorbisComment);
                    info.vorbisComment->blockSize = blockSize;
                    info.vorbisComment->blockType = blockType;
                    info.vorbisComment->isFinal = isLastMetaBlock;
                    break;
                case META_TYPE_PADDING:
                    parsePadding(realData, blockSize, info.padding);
                    info.padding->blockSize = blockSize;
                    info.padding->blockType = blockType;
                    info.padding->isFinal = isLastMetaBlock;
                    break;
                default:
                    break;
                }
                isFinal = isLastMetaBlock;
                return index + blockSize;
            }
            catch (const std::exception &e)
            {
                return PARSE_FAILED;
            }
        }
        static int parseStreamInfo(const char *data, int size, StreamInfo *&info)
        {
            try
            {
                int use = 0;
                short minimumBlock = (data[use] << 8) | data[++use];
                short maximumBlock = (data[++use] << 8) | data[++use];
                int minimumFrame = ((data[++use] & 0x00ff) << 16) | ((data[++use] & 0x00ff) << 8) | (data[++use] & 0x00ff);
                int maximumFrame = ((data[++use] & 0x00ff) << 16) | ((data[++use] & 0x00ff) << 8) | (data[++use] & 0x00ff);
                int sampleRate = ((data[++use] & 0x00ff) << 8) | ((data[++use] & 0x00ff));
                // 取前4位
                sampleRate = (sampleRate << 4) | ((data[++use] & 0x00ff) >> 4);
                // 取5-7位
                short channelsCount = (data[use] >> 1) & 0x07; // 0x00001110
                // 取最后一位并拼接下一个字节的前4位
                short bitsPerSample = ((data[use] & 0x01) << 4) | ((data[++use] & 0x00ff) >> 4);
                info = new StreamInfo();
                info->minimunBlockSize = minimumBlock;
                info->maximumBlockSize = maximumBlock;
                info->minimumFrameSize = minimumFrame;
                info->maximumFrameSize = maximumFrame;
                info->sampleRate = sampleRate;
                info->numberOfChannels = channelsCount;
                info->bitsPerSample = bitsPerSample;
                info->totalSampleCount = (long(data[use] & 0x000f) << 32) | (long(data[++use] & 0x00ff) << 24) | ((data[++use] & 0x00ff) << 16) | ((data[++use] & 0x00ff) << 8) | (data[++use] & 0x00ff);
                use++;
                for (int i = 0; i < size - use; i++)
                {

                    (info)->md5Info[i] = data[use + i];
                }
                return 0;
            }
            catch (const std::exception &e)
            {
                std::cerr << e.what() << '\n';
                return 1;
            }
        }
        static int parsePicture(const char *data, int size, Picture *&picture)
        {
            int use = 0;
            int pictureType = ((data[use] & 0x00ff) << 24) | ((data[++use] & 0x00ff) << 16) | ((data[++use] & 0x00ff) << 8) | (data[++use] & 0x00ff);
            int mineTypeLength = ((data[++use] & 0x00ff) << 24) | ((data[++use] & 0x00ff) << 16) | ((data[++use] & 0x00ff) << 8) | (data[++use] & 0x00ff);
            char mineType[mineTypeLength];
            for (int i = 0; i < mineTypeLength; i++)
            {
                mineType[i] = (data[++use] & 0x00ff);
            }
            int descLength = ((data[++use] & 0x00ff) << 24) | ((data[++use] & 0x00ff) << 16) | ((data[++use] & 0x00ff) << 8) | (data[++use] & 0x00ff);
            char desc[descLength];
            for (int i = 0; i < descLength; i++)
            {
                desc[i] = (data[++use] & 0x00ff);
            }
            int pictureWidth = ((data[++use] & 0x00ff) << 24) | ((data[++use] & 0x00ff) << 16) | ((data[++use] & 0x00ff) << 8) | (data[++use] & 0x00ff);
            int pictureHeight = ((data[++use] & 0x00ff) << 24) | ((data[++use] & 0x00ff) << 16) | ((data[++use] & 0x00ff) << 8) | (data[++use] & 0x00ff);
            int colorDepth = ((data[++use] & 0x00ff) << 24) | ((data[++use] & 0x00ff) << 16) | ((data[++use] & 0x00ff) << 8) | (data[++use] & 0x00ff);
            int colorsUseCount = ((data[++use] & 0x00ff) << 24) | ((data[++use] & 0x00ff) << 16) | ((data[++use] & 0x00ff) << 8) | (data[++use] & 0x00ff);
            int dataLength = ((data[++use] & 0x00ff) << 24) | ((data[++use] & 0x00ff) << 16) | ((data[++use] & 0x00ff) << 8) | (data[++use] & 0x00ff);
            std::string picData;
            for (int i = 0; i < dataLength; i++)
            {
                picData += (data[++use] & 0x00ff);
            }
            picture = new Picture{
                pictureType : pictureType,
                mineTypeLength : mineTypeLength,
                mineType : std::string(mineType),
                descLength : descLength,
                desc : std::string(desc),
                pictureWidth : pictureWidth,
                pictureHeight : pictureHeight,
                colorDepth : colorDepth,
                colorsUseCount : colorsUseCount,
                dataLength : dataLength,
                data : picData
            };
            return 0;
        }
        static int parseSeekTable(const char *data, int size, SeekTable *&table)
        {
            int use = 0;
            int pointSize = 0;
            table = new SeekTable();
            while (use < size)
            {
                SeekPoint point;
                for (int i = 0; i < 8; i++)
                {
                    point.sampleNumber[i] = (data[use++] && 0x00ff);
                }
                for (int i = 0; i < 8; i++)
                {
                    point.frameOffset[i] = (data[use++] && 0x00ff);
                }
                point.targetFrameSampleCount = ((data[use++] && 0x00ff) << 8) | (data[use++] && 0x00ff);
                table->seekPoints->push_back(point);
            }
            return 0;
        }
        static int parseVorbisComment(const char *data, int size, VorbisComment *&vorbisComment)
        {
            int use = 0;
            // 跳过固定格式
            use = +39;
            vorbisComment = new VorbisComment();
            auto &data_map = vorbisComment->dataMap;
            use++;
            while (use < size)
            {
                int length = data[use] & 0x00ff | (data[++use] & 0x00ff) << 8 | (data[++use] & 0x00ff) << 16 | (data[++use] & 0x00ff) << 24;
                std::string key;
                std::string value;
                int flag = 0;
                for (int i = 0; i < length; i++)
                {
                    char code = (data[++use] & 0x00ff);
                    if (code == '=')
                    {

                        flag = 1;
                        continue;
                    }
                    if (flag)
                    {
                        value += code;
                    }
                    else
                    {
                        key += code;
                    }
                }
                use++;
                auto result = data_map->find(key);
                if (result != data_map->end())
                {
                    result->second.push_back(value);
                }
                else
                {
                    std::vector<std::string> values = std::vector<std::string>();
                    values.push_back(value);
                    data_map->insert(std::map<std::string, std::vector<std::string>>::value_type(key, values));
                }
            }
            return 0;
        }
        static int parsePadding(const char *data, int size, Padding *&padding)
        {
            int use = 0;
            char _data[size];
            for (int i = 0; i < size; i++)
            {
                _data[i] = (data[use++] && 0x00ff);
            }
            padding = new Padding{
                data : std::string(_data)
            };
            return 0;
        }
    };

};
#endif