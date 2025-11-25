import std.mem;
import std.io;
import std.hash;
import std.sys;

fn crc8(ref auto pattern){
    return std::hash::crc8(pattern, 0, 0x7, 0, false, false);
};

struct MutexType {
    u8 body[64];
};

struct FileVersionType{
    u8 patch;
    u8 minor;
    u8 major;
    padding[5];
} [[format("print_FileVersionType")]];

fn print_FileVersionType(FileVersionType v){
    return std::format("v{:d}.{:d}.{:d}", v.major, v.minor, v.patch);
};
 
struct FileHeadType {
    char file_magic[16];
    FileVersionType version;
    u64 defintion_offset;
    u64 ringbuffer_offset;
    u64 stack_offset;
    padding[7];
    u8 crc;
    bool valid = !crc8(this) [[export]];
};
FileHeadType FileHead @0;

struct DefinitionSectionType {
    u64 size;
    char name[size];
};
DefinitionSectionType Definition @FileHead.defintion_offset;

enum MetaType : u8 {
    printf = 1,
    dump = 2,
};
struct MetaEntryType{
    u64 file_offset = addressof(this) [[export]];
    char magic;
    std::assert_warn(magic == '{', "invalid magic in meta entry");
    u32 size;
    MetaType type;
    u32 line;
    u8 arg_count;
    char arg_types[arg_count+1];
    char file[];
    char msg[];
    padding[while(!std::mem::eof() && (std::mem::read_unsigned($,1) ==0))];
};

struct TraceEntry<auto payload_size> {
    u48 in_file_offset;
    u32 pid;
    u32 tid;
    u64 timestamp_ns;
    u8 payload[payload_size-22] [[hidden]];
    std::assert_warn(in_file_offset>0, "invalid in_file_offset");
    if(in_file_offset<=0) return;
    if(in_file_offset == 1) return; // dynamic trace entry
    std::assert_warn(in_file_offset > 0xFF, std::format("invalid in_file_offset 0x{:X}", in_file_offset));
    if(in_file_offset <= 0xFF) return;
    
    MetaEntryType *meta : u48 @addressof(in_file_offset);
}[[fixed_size(payload_size)]];

u64 current = 0;
struct RingbufferEntry{
    const u64 base = addressof(parent.raw_body);
    const u64 limit = parent.body_size;
    u64 start = current [[export]];
    
    u8 raw_head[4] @(current%limit+base) [[hidden]];
    char magic@(current%limit+base);
    current += 1 ;
    bool magic_valid = (magic == '~');
    std::assert_warn(magic_valid, "invalid magic");
    if(!magic_valid) return;
    
    u16 payload_size @(current%limit+base);
    current += 2;
    char header_crc @(current%limit+base);
    current += 1;
    bool header_valid = (0 == crc8(raw_head));
    std::assert_warn(header_valid, "invalid header");
    if(!header_valid) return;
  
    u8 raw_payload[payload_size] @(current%limit+base) [[hidden]];
    current += payload_size ;
    char payload_crc @(current%limit+base);
    current += 1;
    bool payload_valid = (payload_crc == crc8(raw_payload));
    std::assert_warn(payload_valid, "invalid payload");
    if(!payload_valid) return;
    
    TraceEntry<payload_size> payload @addressof(raw_payload);
};

struct RingbufferSectionType {
    u64 version;
    MutexType mutext;
    u64 body_size;
    u64 wrapped;
    u64 dropped;
    u64 entries_count;
    u64 next_free;
    u64 last_valid;
    padding[40]; // future use
    u8 raw_body[body_size] [[hidden]];
    
    const u64 expected_entry_count = (entries_count - dropped) [[export]];
    current = last_valid; 
    u64 end = (last_valid <= next_free) ? next_free : (next_free + body_size);
    RingbufferEntry entries[while(current < end)] [[no_uniqe_address]];
};
RingbufferSectionType Ringbuffer @FileHead.ringbuffer_offset;


struct StackSectionEntryType{
    u128 md5_hash;
    padding[8];
    u32 body_size;
    u8 crc;
    u64 end = (addressof(crc) + sizeof(crc) + body_size);
    MetaEntryType meta_entries[while(!std::mem::reached(end))];
}[[fixed_size(29+body_size)]];

struct StackSectionType{
    u64 version;
    MutexType mutext;
    padding[40];  // future use
    u64 body_size;
    StackSectionEntryType entries[while(!std::mem::eof())];
}[[fixed_size(120+body_size)]];
StackSectionType Stack @FileHead.stack_offset;