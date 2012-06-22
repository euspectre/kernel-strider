/* 
 * Test that metadata may be read in packet form.
 */

#include <kedr/ctf_reader/ctf_reader.h>

#include <stdexcept>
#include <cassert>
#include <iostream>
#include <fstream>

#include <endian.h>

#include <sstream>

#include <cassert>
#include <cstring> /* memcpy*/

std::string sourceDir;

static int test1(void);
static int test2(void);

/* 
 * If test is running not from its source directory,
 * first argument should contain this source directory.
 */
int main(int argc, char *argv[])
{
    if(argc > 1)
    {
        sourceDir = argv[1];
        sourceDir += '/';
    }

    int result;

#define RUN_TEST(test_func, test_name) do {\
    try {result = test_func(); }\
    catch(std::exception& e) \
    { \
        std::cerr << "Exception occures in '" << test_name << "': " \
            << e.what() << "." << std::endl; \
        return 1; \
    } \
    if(result) return result; \
}while(0)

    RUN_TEST(test1, "big endian form");
    RUN_TEST(test2, "little endian form");

    return 0;
}

/* 
 * Pack metadata, contained in the 'in' stream, into some packet form.
 * Result is written into 'out' stream.
 * 'isBE' parameter affects on endianess of metadata packet header.
 */
static void packMetadata(std::istream& in, std::ostream& out, bool isBE);

/* 
 * Read correct metadata and then pack it into packet form.
 * Then try to read packets.
 */
int testCommon(bool isBE)
{
    std::string metaFilename = sourceDir + "metadata";
    
    std::ifstream fs;
    fs.open(metaFilename.c_str());
    if(!fs)
    {
        std::cerr << "Failed to open file '" << metaFilename
        << "' with metadata." <<std::endl;
        return 1;
    }
    /* Initial value of metadata in string */
    std::stringstream metadataInitial;
    /* Read file with metadata into string*/
    while(fs.peek() != std::ifstream::traits_type::eof())
    {
        metadataInitial.put(fs.get());
    }
    /* Packet representation of metadata */
    std::stringstream metadataPackets;
    
    packMetadata(metadataInitial, metadataPackets, isBE);
    
    /* Unpacked metadata */
    std::stringstream metadataUnpacked;
    int nPackets = 0;
    for(CTFReader::MetaPacketIterator iter(metadataPackets);
        iter;
        ++iter)
    {
        metadataUnpacked.write(iter->getMetadata(), iter->getMetadataSize());
        nPackets++;
        //std::cerr << "Packet " << nPackets << " is unpacked.\n";
    }
    //std::cerr << "Number of packets is " << nPackets << ".\n";
    
    
    if(metadataInitial.str() != metadataUnpacked.str())
    {
        std::cerr << "Unpacked metadata differs from original.\n";
        std::cerr << std::endl;
        std::cerr << "******************* Original metadata ********************\n";
        std::cerr << metadataInitial.str() << std::endl;
        std::cerr << "***************** Original metadata ends *****************\n";
        std::cerr << "******************* Unpacket metadata ********************\n";
        std::cerr << metadataUnpacked.str() << std::endl;
        std::cerr << "***************** Unpacket metadata ends *****************\n";
        return 1;
    }
    return 0;
}


int test1(void)
{
    return testCommon(true);
}

int test2(void)
{
    return testCommon(false);
}    


/* 
 * Fill header for metadata packet.
 * 
 * chunkSize is size(in bytes) of metadata packed.
 * paddingSize is size(in bytes) of padding after metadata.
 * 'isBE' parameter affects on endianess.
 */
static void fillMetaHeader(char header[37], int chunkSize,
    int paddingSize, bool isBE);

/* 
 * Similar to read() system call for files.
 */
static int readFromStream(std::istream& in, char* buf, int size);

void packMetadata(std::istream& in, std::ostream& out, bool isBE)
{
    static char header[37];
    /* Buffer for metadata chunks */
    static char chunk[67];
    /* Padding of each packet will be calculated using this value */
    static size_t paddingBase = 0;
    
    int packetIndex = 0;
    for(;;packetIndex++)
    {
        size_t chunkSize;
        size_t padding = paddingBase;
        /* Pseudo random chunk sizes and padding */
        if(packetIndex % 2)
        {
            chunkSize = 67;
        }
        else
        {
            chunkSize = 40;
        }
        assert(chunkSize <= sizeof(chunk));
        
        switch(packetIndex % 3)
        {
        case 0:
        break;
        case 1:
            padding += 1;
        break;
        case 2:
            padding += 10;
        break;
        }
        
        chunkSize = readFromStream(in, chunk, chunkSize);
        if(chunkSize <= 0) break;
        
        fillMetaHeader(header, chunkSize, padding, isBE);
        
        out.write(header, sizeof(header));
        out.write(chunk, chunkSize);
        if(!out)
        {
            throw std::runtime_error("Failed to write meta packet.");
        }
        for(size_t i = 0; i < padding; i++)
            out.put('\0');
        if(!out){
            throw std::runtime_error("Failed to pad meta packet.");
        }
    }
    //std::cerr << "Number of created packets is " << packetIndex << ".\n";
}

void fillMetaHeader(char header[37], int chunkSize,
    int paddingSize, bool isBE)
{
    static uint8_t uuid[16] = {1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6};
    
    CTFMetadataPacketHeader& headerReal = *((CTFMetadataPacketHeader*)header);
    
    memcpy(headerReal.uuid, uuid, 16);
#define FIELD32(val) (isBE ? htobe32(val) : htole32(val))
    headerReal.magic = FIELD32(CTFMetadataPacketHeader::magicValue);
    
    uint32_t contentSize = CTFMetadataPacketHeader::headerSize * 8
        + chunkSize * 8;
    uint32_t packetSize = contentSize + paddingSize * 8;
    
    headerReal.content_size = FIELD32(contentSize);
    headerReal.packet_size = FIELD32(packetSize);
#undef FIELD32    
    headerReal.checksum = 0;
    headerReal.compression_scheme = 0;
    headerReal.encryption_scheme = 0;
    headerReal.checksum_scheme = 0;
    headerReal.major = CTFMetadataPacketHeader::majorValue;
    headerReal.minor = CTFMetadataPacketHeader::minorValue;
}

int readFromStream(std::istream& in, char* buf, int size)
{
    int sizeReal;
    for(sizeReal = 0; sizeReal < size; sizeReal++)
    {
        if(in.peek() == std::istream::traits_type::eof()) break;
        in.get(buf[sizeReal]);
    }
    //std::cerr << "Read " << sizeReal << " bytes from stream(need "
    //    << size << ").\n";
    return sizeReal;
}