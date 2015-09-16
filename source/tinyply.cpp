// This software is in the public domain. Where that dedication is not
// recognized, you are granted a perpetual, irrevocable license to copy,
// distribute, and modify this file as you see fit.
// Authored in 2015 by Dimitri Diakopoulos (http://www.dimitridiakopoulos.com)
// https://github.com/ddiakopoulos/tinyply

#include "tinyply.h"

using namespace tinyply;
using namespace std;

//////////////////
// PLY Property //
//////////////////

PlyProperty::PlyProperty(std::istream & is) : isList(false)
{
    parse_internal(is);
}

void PlyProperty::parse_internal(std::istream & is)
{
    string type;
    is >> type;
    if (type == "list")
    {
        string countType;
        is >> countType >> type;
        listType = get_data_type(countType);
        isList = true;
    }
    propertyType = get_data_type(type);
    is >> name;
}

PlyProperty::Type PlyProperty::get_data_type(const string & t)
{
    if      (t == "int8"    || t == "char")     return PlyProperty::Type::INT8;
    else if (t == "uint8"   || t == "uchar")    return PlyProperty::Type::UINT8;
    else if (t == "int16"   || t == "short")    return PlyProperty::Type::INT16;
    else if (t == "uint16"  || t == "ushort")   return PlyProperty::Type::UINT16;
    else if (t == "int32"   || t == "int")      return PlyProperty::Type::INT32;
    else if (t == "uint32"  || t == "uint")     return PlyProperty::Type::UINT32;
    else if (t == "float32" || t == "float")    return PlyProperty::Type::FLOAT32;
    else if (t == "float64" || t == "double")   return PlyProperty::Type::FLOAT64;
    return PlyProperty::Type::INVALID;
}

/////////////////
// PLY Element //
/////////////////

PlyElement::PlyElement(std::istream& is)
{
    parse_internal(is);
}

void PlyElement::parse_internal(std::istream& is)
{
    is >> name >> size;
}

//////////////
// PLY File //
//////////////

PlyFile::PlyFile(std::istream& is)
{
    parse_header(is);
}

bool PlyFile::parse_header(std::istream& is)
{
    std::string line;
    while (std::getline(is, line))
    {
        std::istringstream ls(line);
        std::string token;
        ls >> token;
        if (token == "ply" || token == "PLY" || token == "")
            continue;
        else if (token == "comment")
            read_header_text(line, ls, comments, 7);
        else if (token == "format")
            read_header_format(ls);
        else if (token == "element")
            read_header_element(ls);
        else if (token == "property")
            read_header_property(ls);
        else if (token == "obj_info")
            read_header_text(line, ls, objInfo, 7);
        else if (token == "end_header")
            break;
        else
            return false;
    }
    return true;
}

void PlyFile::read_header_text(std::string line, std::istream & is, std::vector<std::string> place, int erase)
{
    place.push_back((erase > 0) ? line.erase(0, erase) : line);
}

void PlyFile::read_header_format(std::istream & is)
{
    std::string s;
    (is >> s);
    if (s == "ascii" || s == "ASCII")
        isBinary = false;
    else if (s == "binary_little_endian" || s == "binary_big_endian")
        isBinary = true;
}

void PlyFile::read_header_element(std::istream & is)
{
    PlyElement e(is);
    get_elements().push_back(e);
}

void PlyFile::read_header_property(std::istream& is)
{
    PlyProperty e(is);
    get_elements().back().get_properties().push_back(e);
}

void PlyFile::parse(std::istream & is, const std::vector<uint8_t> & buffer)
{
    if (isBinary) parse_data_binary(is, buffer);
    else parse_data_ascii(is, buffer);
}

void PlyFile::parse_data_binary(std::istream & is, const std::vector<uint8_t> & buffer)
{
    uint32_t fileOffset = 0;
    const size_t headerPosition = is.tellg();
    const std::uint8_t * srcBuffer = buffer.data() + headerPosition;
    
    for (auto & element : get_elements())
    {
        if (std::find(requestedElements.begin(), requestedElements.end(), element.get_name()) != requestedElements.end())
        {
            for (int64_t count = 0; count < element.get_element_count(); ++count)
            {
                for (const auto & property : element.get_properties())
                {
                    auto token = property.get_name();
                    auto listType = property.get_list_type();
                    auto propertyType = property.get_property_type();
                    
                    if (userDataMap[token])
                    {
                        auto & cursor = userDataMap[token];
                        if (property.is_list())
                        {
                            uint32_t listSize = 0;
                            size_t dummyCount = 0;
                            read_property(listType, &listSize, dummyCount, srcBuffer, fileOffset);
                            for (int i = 0; i < listSize; ++i)
                                read_property(propertyType, (cursor->data + cursor->offset), cursor->offset, srcBuffer, fileOffset);
                        }
                        else
                        {
                            read_property(propertyType, (cursor->data + cursor->offset), cursor->offset, srcBuffer, fileOffset);
                        }
                    }
                    else
                    {
                        skip_property(fileOffset, property, srcBuffer);
                    }
                }
            }
        }
        else continue;
    }
}

void PlyFile::parse_data_ascii(std::istream & is, const std::vector<uint8_t> & buffer)
{
    for (auto & element : get_elements())
    {
        if (std::find(requestedElements.begin(), requestedElements.end(), element.get_name()) != requestedElements.end())
        {
            for (int64_t count = 0; count < element.get_element_count(); ++count)
            {
                for (const auto & property : element.get_properties())
                {
                    auto token = property.get_name();
                    auto listType = property.get_list_type();
                    auto propertyType = property.get_property_type();
                    if (userDataMap[token])
                    {
                        auto & cursor = userDataMap[token];
                        if (property.is_list())
                        {
                            uint32_t listSize = 0;
                            size_t dummyCount = 0;
                            read_property(listType, &listSize, dummyCount, is);
                            for (int i = 0; i < listSize; ++i)
                            {
                                read_property(propertyType, (cursor->data + cursor->offset), cursor->offset, is);
                            }
                        }
                        else
                        {
                            read_property(propertyType, (cursor->data + cursor->offset), cursor->offset, is);
                        }
                    }
                    else
                    {
                        skip_property(is, property);
                    }
                }
            }
        }
        else continue;
    }
}