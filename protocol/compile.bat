protoc --descriptor_set_out=protocol.protobin --include_imports protocol.proto
protogen protocol.protobin
protoc.exe -I="." --cpp_out=. protocol.proto