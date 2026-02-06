#pragma once

#include "../types.hpp"

namespace sertos::apps {

constexpr u32 HTML_MAX_ELEMENTS = 512;
constexpr u32 HTML_MAX_TEXT_LENGTH = 256;
constexpr u32 HTML_MAX_ATTR_LENGTH = 256;

enum class HtmlTag : u8 {
    None = 0,
    Html, Head, Title, Body,
    H1, H2, H3, H4, H5, H6,
    P, Br, Hr,
    A, B, Strong, I, Em, U,
    Ul, Ol, Li,
    Pre, Code, Blockquote,
    Div, Span,
    Img,
    Table, Tr, Td, Th,
    Text,
    Unknown
};

struct HtmlElement {
    HtmlTag tag;
    char text[HTML_MAX_TEXT_LENGTH];
    char href[HTML_MAX_ATTR_LENGTH];
    char alt[HTML_MAX_ATTR_LENGTH];
    u32  textLength;
    bool isClosing;
    bool isSelfClosing;
};

struct RenderLine {
    char text[HTML_MAX_TEXT_LENGTH];
    u32  textLength;
    u8   colorR, colorG, colorB;
    bool bold;
    bool italic;
    bool underline;
    bool isLink;
    char linkHref[HTML_MAX_ATTR_LENGTH];
    u32  indent;
    bool isHr;
    u32  spacingBefore;
};

constexpr u32 MAX_RENDER_LINES = 2048;
constexpr u32 MAX_LINKS = 256;

struct ClickableLink {
    u32 lineIndex;
    u32 startCol, endCol;
    char href[HTML_MAX_ATTR_LENGTH];
};

class HtmlParser {
public:
    static u32 parse(const char* html, u32 htmlLength, HtmlElement* elements, u32 maxElements);

    static u32 layout(const HtmlElement* elements, u32 elementCount,
                      RenderLine* lines, u32 maxLines, u32 widthChars,
                      ClickableLink* links, u32 maxLinks, u32* linkCount,
                      char* titleOut, u32 titleMaxLen);

private:
    static HtmlTag identifyTag(const char* tagName, u32 length);
    static bool parseTag(const char* html, u32 pos, u32 length,
                         HtmlElement* element, u32* endPos);
    static void extractAttribute(const char* tag, u32 tagLen,
                                  const char* attrName, char* value, u32 maxValue);
    static void decodeEntities(const char* src, u32 srcLen, char* dest, u32 maxLen, u32* outLen);
    static bool isBlockTag(HtmlTag tag);
};

}
