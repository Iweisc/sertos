#include "../../include/apps/html_parser.hpp"

namespace sertos::apps {

// --- String helpers (freestanding, no libc) ---

static u32 slen(const char* s) {
    u32 len = 0;
    while (s[len]) len++;
    return len;
}

static void scpy(char* dst, const char* src, u32 maxLen) {
    if (maxLen == 0) return;
    u32 i = 0;
    while (src[i] && i < maxLen - 1) { dst[i] = src[i]; i++; }
    dst[i] = '\0';
}

static void sncpy(char* dst, const char* src, u32 n, u32 maxLen) {
    if (maxLen == 0) return;
    u32 limit = n < maxLen - 1 ? n : maxLen - 1;
    u32 i = 0;
    while (i < limit && src[i]) { dst[i] = src[i]; i++; }
    dst[i] = '\0';
}

static bool strieqn(const char* a, const char* b, u32 n) {
    for (u32 i = 0; i < n; i++) {
        char ca = a[i], cb = b[i];
        if (ca >= 'A' && ca <= 'Z') ca += 32;
        if (cb >= 'A' && cb <= 'Z') cb += 32;
        if (ca != cb) return false;
        if (ca == '\0') return true;
    }
    return true;
}

static bool isSpace(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

// --- Tag identification ---

struct TagNameEntry {
    const char* name;
    HtmlTag tag;
};

static const TagNameEntry TAG_NAMES[] = {
    {"html", HtmlTag::Html}, {"head", HtmlTag::Head}, {"title", HtmlTag::Title},
    {"body", HtmlTag::Body}, {"h1", HtmlTag::H1}, {"h2", HtmlTag::H2},
    {"h3", HtmlTag::H3}, {"h4", HtmlTag::H4}, {"h5", HtmlTag::H5},
    {"h6", HtmlTag::H6}, {"p", HtmlTag::P}, {"br", HtmlTag::Br},
    {"hr", HtmlTag::Hr}, {"a", HtmlTag::A}, {"b", HtmlTag::B},
    {"strong", HtmlTag::Strong}, {"i", HtmlTag::I}, {"em", HtmlTag::Em},
    {"u", HtmlTag::U}, {"ul", HtmlTag::Ul}, {"ol", HtmlTag::Ol},
    {"li", HtmlTag::Li}, {"pre", HtmlTag::Pre}, {"code", HtmlTag::Code},
    {"blockquote", HtmlTag::Blockquote}, {"div", HtmlTag::Div},
    {"span", HtmlTag::Span}, {"img", HtmlTag::Img},
    {"table", HtmlTag::Table}, {"tr", HtmlTag::Tr},
    {"td", HtmlTag::Td}, {"th", HtmlTag::Th},
};

static constexpr u32 TAG_NAME_COUNT = sizeof(TAG_NAMES) / sizeof(TAG_NAMES[0]);

HtmlTag HtmlParser::identifyTag(const char* tagName, u32 length) {
    for (u32 i = 0; i < TAG_NAME_COUNT; i++) {
        u32 nameLen = slen(TAG_NAMES[i].name);
        if (nameLen == length && strieqn(tagName, TAG_NAMES[i].name, length)) {
            return TAG_NAMES[i].tag;
        }
    }
    return HtmlTag::Unknown;
}

// --- Attribute extraction ---

void HtmlParser::extractAttribute(const char* tag, u32 tagLen,
                                    const char* attrName, char* value, u32 maxValue) {
    value[0] = '\0';
    u32 attrNameLen = slen(attrName);

    for (u32 i = 0; i + attrNameLen < tagLen; i++) {
        if (i > 0 && !isSpace(tag[i - 1])) continue;
        if (strieqn(tag + i, attrName, attrNameLen) && tag[i + attrNameLen] == '=') {
            u32 pos = i + attrNameLen + 1;
            char quote = '\0';
            if (pos < tagLen && (tag[pos] == '"' || tag[pos] == '\'')) {
                quote = tag[pos];
                pos++;
            }
            u32 start = pos;
            while (pos < tagLen) {
                if (quote && tag[pos] == quote) break;
                if (!quote && (isSpace(tag[pos]) || tag[pos] == '>')) break;
                pos++;
            }
            u32 len = pos - start;
            sncpy(value, tag + start, len, maxValue);
            return;
        }
    }
}

// --- Entity decoding ---

void HtmlParser::decodeEntities(const char* src, u32 srcLen, char* dest, u32 maxLen, u32* outLen) {
    if (maxLen == 0) { if (outLen) *outLen = 0; return; }
    u32 si = 0, di = 0;
    while (si < srcLen && di < maxLen - 1) {
        if (src[si] == '&') {
            // Try to match an entity
            if (si + 3 < srcLen && strieqn(src + si, "&lt;", 4)) {
                dest[di++] = '<'; si += 4;
            } else if (si + 3 < srcLen && strieqn(src + si, "&gt;", 4)) {
                dest[di++] = '>'; si += 4;
            } else if (si + 4 < srcLen && strieqn(src + si, "&amp;", 5)) {
                dest[di++] = '&'; si += 5;
            } else if (si + 5 < srcLen && strieqn(src + si, "&quot;", 6)) {
                dest[di++] = '"'; si += 6;
            } else if (si + 5 < srcLen && strieqn(src + si, "&apos;", 6)) {
                dest[di++] = '\''; si += 6;
            } else if (si + 5 < srcLen && strieqn(src + si, "&nbsp;", 6)) {
                dest[di++] = ' '; si += 6;
            } else if (si + 2 < srcLen && src[si + 1] == '#') {
                // Numeric entity &#NNN; or &#xHH;
                u32 val = 0;
                u32 ep = si + 2;
                bool hex = false;
                if (ep < srcLen && (src[ep] == 'x' || src[ep] == 'X')) {
                    hex = true; ep++;
                }
                while (ep < srcLen && src[ep] != ';') {
                    char c = src[ep];
                    if (hex) {
                        if (c >= '0' && c <= '9') val = val * 16 + (c - '0');
                        else if (c >= 'a' && c <= 'f') val = val * 16 + (c - 'a' + 10);
                        else if (c >= 'A' && c <= 'F') val = val * 16 + (c - 'A' + 10);
                        else break;
                    } else {
                        if (c >= '0' && c <= '9') val = val * 10 + (c - '0');
                        else break;
                    }
                    ep++;
                }
                if (ep < srcLen && src[ep] == ';') ep++;
                if (val > 0 && val < 128) dest[di++] = static_cast<char>(val);
                else dest[di++] = '?';
                si = ep;
            } else {
                dest[di++] = src[si++];
            }
        } else {
            dest[di++] = src[si++];
        }
    }
    dest[di] = '\0';
    if (outLen) *outLen = di;
}

// --- Tag parsing ---

bool HtmlParser::parseTag(const char* html, u32 pos, u32 length,
                           HtmlElement* element, u32* endPos) {
    if (pos >= length || html[pos] != '<') return false;

    u32 tagStart = pos + 1;
    bool isClosing = false;

    // Skip whitespace
    while (tagStart < length && isSpace(html[tagStart])) tagStart++;

    if (tagStart < length && html[tagStart] == '/') {
        isClosing = true;
        tagStart++;
    }

    // Find tag name end
    u32 nameStart = tagStart;
    while (tagStart < length && !isSpace(html[tagStart]) &&
           html[tagStart] != '>' && html[tagStart] != '/') {
        tagStart++;
    }

    u32 nameLen = tagStart - nameStart;
    if (nameLen == 0) return false;

    element->tag = identifyTag(html + nameStart, nameLen);
    element->isClosing = isClosing;
    element->isSelfClosing = false;
    element->text[0] = '\0';
    element->href[0] = '\0';
    element->alt[0] = '\0';
    element->textLength = 0;

    // Find the closing '>'
    u32 tagEnd = tagStart;
    while (tagEnd < length && html[tagEnd] != '>') tagEnd++;

    if (tagEnd < length) {
        // Check self-closing
        if (tagEnd > 0 && html[tagEnd - 1] == '/') {
            element->isSelfClosing = true;
        }

        // Extract attributes from the full tag content
        u32 attrLen = tagEnd - (pos + 1);
        if (attrLen > 0) {
            extractAttribute(html + pos + 1, attrLen, "href", element->href, HTML_MAX_ATTR_LENGTH);
            extractAttribute(html + pos + 1, attrLen, "alt", element->alt, HTML_MAX_ATTR_LENGTH);
        }

        // br, hr, img are self-closing by nature
        if (element->tag == HtmlTag::Br || element->tag == HtmlTag::Hr ||
            element->tag == HtmlTag::Img) {
            element->isSelfClosing = true;
        }

        *endPos = tagEnd + 1;
        return true;
    }

    *endPos = length;
    return false;
}

bool HtmlParser::isBlockTag(HtmlTag tag) {
    switch (tag) {
        case HtmlTag::P: case HtmlTag::Div: case HtmlTag::H1:
        case HtmlTag::H2: case HtmlTag::H3: case HtmlTag::H4:
        case HtmlTag::H5: case HtmlTag::H6: case HtmlTag::Ul:
        case HtmlTag::Ol: case HtmlTag::Li: case HtmlTag::Blockquote:
        case HtmlTag::Pre: case HtmlTag::Hr: case HtmlTag::Table:
        case HtmlTag::Tr:
            return true;
        default:
            return false;
    }
}

// --- Main parser ---

u32 HtmlParser::parse(const char* html, u32 htmlLength, HtmlElement* elements, u32 maxElements) {
    u32 count = 0;
    u32 pos = 0;

    // Track whether we're inside <script> or <style>
    bool inScript = false;
    bool inStyle = false;

    while (pos < htmlLength && count < maxElements) {
        // Skip inside script/style
        if (inScript) {
            // Search for </script>
            while (pos + 8 <= htmlLength) {
                if (html[pos] == '<' && html[pos + 1] == '/' &&
                    strieqn(html + pos + 2, "script", 6)) {
                    // Skip past </script>
                    while (pos < htmlLength && html[pos] != '>') pos++;
                    if (pos < htmlLength) pos++;
                    inScript = false;
                    break;
                }
                pos++;
            }
            if (inScript) break;
            continue;
        }
        if (inStyle) {
            while (pos + 7 <= htmlLength) {
                if (html[pos] == '<' && html[pos + 1] == '/' &&
                    strieqn(html + pos + 2, "style", 5)) {
                    while (pos < htmlLength && html[pos] != '>') pos++;
                    if (pos < htmlLength) pos++;
                    inStyle = false;
                    break;
                }
                pos++;
            }
            if (inStyle) break;
            continue;
        }

        if (html[pos] == '<') {
            // Check for comment
            if (pos + 3 < htmlLength && html[pos + 1] == '!' &&
                html[pos + 2] == '-' && html[pos + 3] == '-') {
                // Skip comment
                pos += 4;
                while (pos + 2 < htmlLength) {
                    if (html[pos] == '-' && html[pos + 1] == '-' && html[pos + 2] == '>') {
                        pos += 3;
                        break;
                    }
                    pos++;
                }
                continue;
            }

            // Check for <!DOCTYPE etc
            if (pos + 1 < htmlLength && html[pos + 1] == '!') {
                while (pos < htmlLength && html[pos] != '>') pos++;
                if (pos < htmlLength) pos++;
                continue;
            }

            // Check for script/style opening tags
            u32 checkPos = pos + 1;
            while (checkPos < htmlLength && isSpace(html[checkPos])) checkPos++;

            if (checkPos + 6 <= htmlLength && strieqn(html + checkPos, "script", 6)) {
                char after = (checkPos + 6 < htmlLength) ? html[checkPos + 6] : '>';
                if (isSpace(after) || after == '>') {
                    while (pos < htmlLength && html[pos] != '>') pos++;
                    if (pos < htmlLength) pos++;
                    inScript = true;
                    continue;
                }
            }
            if (checkPos + 5 <= htmlLength && strieqn(html + checkPos, "style", 5)) {
                char after = (checkPos + 5 < htmlLength) ? html[checkPos + 5] : '>';
                if (isSpace(after) || after == '>') {
                    while (pos < htmlLength && html[pos] != '>') pos++;
                    if (pos < htmlLength) pos++;
                    inStyle = true;
                    continue;
                }
            }

            // Parse regular tag
            HtmlElement* elem = &elements[count];
            u32 endPos = 0;
            if (parseTag(html, pos, htmlLength, elem, &endPos)) {
                count++;
                pos = endPos;
            } else {
                // Use endPos from parseTag to skip ahead. When parseTag
                // scanned to the end of the buffer looking for '>', endPos
                // is set to htmlLength. Advancing only by 1 here would cause
                // O(n^2) behavior when many unclosed '<' chars each trigger
                // a full scan to the end. Fall back to pos+1 only if endPos
                // was not advanced (e.g. nameLen==0 early-return).
                pos = endPos > pos ? endPos : pos + 1;
            }
        } else {
            // Text content - collect until next '<'
            u32 textStart = pos;
            while (pos < htmlLength && html[pos] != '<') pos++;

            u32 rawLen = pos - textStart;
            if (rawLen > 0) {
                HtmlElement* elem = &elements[count];
                elem->tag = HtmlTag::Text;
                elem->isClosing = false;
                elem->isSelfClosing = false;
                elem->href[0] = '\0';
                elem->alt[0] = '\0';

                decodeEntities(html + textStart, rawLen, elem->text, HTML_MAX_TEXT_LENGTH, &elem->textLength);
                count++;
            }
        }
    }

    return count;
}

// --- Layout engine ---

u32 HtmlParser::layout(const HtmlElement* elements, u32 elementCount,
                        RenderLine* lines, u32 maxLines, u32 widthChars,
                        ClickableLink* links, u32 maxLinks, u32* linkCount,
                        char* titleOut, u32 titleMaxLen) {

    u32 lineCount = 0;
    u32 lnkCount = 0;
    if (titleOut) titleOut[0] = '\0';

    // State
    bool bold = false;
    bool italic = false;
    bool underline = false;
    bool inLink = false;
    bool inTitle = false;
    bool inHead = false;
    bool inPre = false;
    u8 colorR = 0, colorG = 0, colorB = 0;
    char linkHref[HTML_MAX_ATTR_LENGTH] = {0};
    u32 linkStartCol = 0;
    u32 indent = 0;
    u32 listDepth = 0;
    u32 orderedCount = 0;
    bool needSpacing = false;

    // Formatting state stack for save/restore across nested tags
    struct FmtState { u8 r, g, b; bool bold, italic, underline; };
    FmtState fmtStack[8];
    u32 fmtDepth = 0;

    auto pushFmt = [&]() {
        if (fmtDepth < 8) {
            fmtStack[fmtDepth] = {colorR, colorG, colorB, bold, italic, underline};
            fmtDepth++;
        }
    };

    auto popFmt = [&]() {
        if (fmtDepth > 0) {
            fmtDepth--;
            colorR = fmtStack[fmtDepth].r;
            colorG = fmtStack[fmtDepth].g;
            colorB = fmtStack[fmtDepth].b;
            bold = fmtStack[fmtDepth].bold;
            italic = fmtStack[fmtDepth].italic;
            underline = fmtStack[fmtDepth].underline;
        }
    };

    // Current line buffer
    char curLine[HTML_MAX_TEXT_LENGTH];
    u32 curLineLen = 0;
    curLine[0] = '\0';

    auto flushLine = [&](bool forceEmpty = false) {
        if (lineCount >= maxLines) return;
        if (curLineLen == 0 && !forceEmpty) return;

        RenderLine* rl = &lines[lineCount];
        sncpy(rl->text, curLine, curLineLen, HTML_MAX_TEXT_LENGTH);
        rl->textLength = curLineLen;
        rl->colorR = colorR;
        rl->colorG = colorG;
        rl->colorB = colorB;
        rl->bold = bold;
        rl->italic = italic;
        rl->underline = underline || inLink;
        rl->isLink = inLink;
        if (inLink) scpy(rl->linkHref, linkHref, HTML_MAX_ATTR_LENGTH);
        else rl->linkHref[0] = '\0';
        rl->indent = indent;
        rl->isHr = false;
        rl->spacingBefore = needSpacing ? 1 : 0;
        needSpacing = false;

        // Record link
        if (inLink && curLineLen > 0 && lnkCount < maxLinks) {
            ClickableLink* lk = &links[lnkCount];
            lk->lineIndex = lineCount;
            lk->startCol = indent + linkStartCol;
            lk->endCol = indent + curLineLen;
            scpy(lk->href, linkHref, HTML_MAX_ATTR_LENGTH);
            lnkCount++;
        }

        lineCount++;
        curLine[0] = '\0';
        curLineLen = 0;
        linkStartCol = 0;
    };

    auto emitHr = [&]() {
        flushLine();
        if (lineCount >= maxLines) return;
        RenderLine* rl = &lines[lineCount];
        rl->text[0] = '\0';
        rl->textLength = 0;
        rl->colorR = 180; rl->colorG = 180; rl->colorB = 180;
        rl->bold = false; rl->italic = false; rl->underline = false;
        rl->isLink = false; rl->linkHref[0] = '\0';
        rl->indent = 0; rl->isHr = true; rl->spacingBefore = 0;
        lineCount++;
    };

    auto addWord = [&](const char* word, u32 wordLen) {
        if (wordLen == 0) return;
        u32 effectiveWidth = widthChars > indent ? widthChars - indent : 1;
        // If adding this word would overflow, flush and start new line
        if (curLineLen > 0 && curLineLen + 1 + wordLen > effectiveWidth) {
            flushLine();
        }
        // Add space separator if line already has content
        if (curLineLen > 0 && curLineLen < HTML_MAX_TEXT_LENGTH - 1) {
            curLine[curLineLen++] = ' ';
            curLine[curLineLen] = '\0';
        }
        // Break long words that exceed line width
        while (wordLen > 0) {
            u32 bufAvail = (HTML_MAX_TEXT_LENGTH - 1 > curLineLen) ? HTML_MAX_TEXT_LENGTH - 1 - curLineLen : 0;
            u32 lineAvail = effectiveWidth > curLineLen ? effectiveWidth - curLineLen : 0;
            // If no space left on current line, flush and recalculate
            if (lineAvail == 0) {
                flushLine();
                bufAvail = HTML_MAX_TEXT_LENGTH - 1;
                lineAvail = effectiveWidth > 0 ? effectiveWidth : 1;
            }
            u32 chunk = wordLen < lineAvail ? wordLen : lineAvail;
            if (chunk > bufAvail) chunk = bufAvail;
            if (chunk == 0) break; // safety: avoid infinite loop
            for (u32 i = 0; i < chunk; i++) {
                curLine[curLineLen++] = word[i];
            }
            curLine[curLineLen] = '\0';
            word += chunk;
            wordLen -= chunk;
        }
    };

    auto addText = [&](const char* text, u32 textLen) {
        if (inPre) {
            // In preformatted mode, preserve whitespace and newlines
            for (u32 i = 0; i < textLen; i++) {
                if (text[i] == '\n') {
                    flushLine(true);
                } else {
                    if (curLineLen < HTML_MAX_TEXT_LENGTH - 1) {
                        curLine[curLineLen++] = text[i];
                        curLine[curLineLen] = '\0';
                    }
                    u32 effectiveWidth = widthChars > indent ? widthChars - indent : 1;
                    if (curLineLen >= effectiveWidth) flushLine();
                }
            }
            return;
        }

        // Word-wrap mode
        u32 i = 0;
        while (i < textLen) {
            // Skip whitespace
            while (i < textLen && isSpace(text[i])) i++;
            if (i >= textLen) break;

            // Find word end
            u32 wordStart = i;
            while (i < textLen && !isSpace(text[i])) i++;
            u32 wordLen = i - wordStart;

            addWord(text + wordStart, wordLen);
        }
    };

    auto setHeadingStyle = [&](HtmlTag tag) {
        bold = true;
        needSpacing = true;
        switch (tag) {
            case HtmlTag::H1: colorR = 30; colorG = 80; colorB = 200; break;
            case HtmlTag::H2: colorR = 40; colorG = 100; colorB = 180; break;
            case HtmlTag::H3: colorR = 50; colorG = 110; colorB = 160; break;
            case HtmlTag::H4: colorR = 60; colorG = 60; colorB = 140; break;
            case HtmlTag::H5: colorR = 70; colorG = 70; colorB = 130; break;
            case HtmlTag::H6: colorR = 80; colorG = 80; colorB = 120; break;
            default: break;
        }
    };

    for (u32 ei = 0; ei < elementCount; ei++) {
        const HtmlElement* elem = &elements[ei];

        if (elem->tag == HtmlTag::Text) {
            if (inHead && !inTitle) continue; // Skip non-title head content

            if (inTitle && titleOut) {
                // Capture title text
                u32 tLen = slen(titleOut);
                u32 avail = titleMaxLen - tLen - 1;
                if (avail > 0) {
                    u32 toCopy = elem->textLength < avail ? elem->textLength : avail;
                    for (u32 i = 0; i < toCopy; i++) {
                        titleOut[tLen + i] = elem->text[i];
                    }
                    titleOut[tLen + toCopy] = '\0';
                }
                continue;
            }

            addText(elem->text, elem->textLength);
            continue;
        }

        if (elem->isClosing) {
            switch (elem->tag) {
                case HtmlTag::Head:
                    inHead = false;
                    break;
                case HtmlTag::Title:
                    inTitle = false;
                    break;
                case HtmlTag::H1: case HtmlTag::H2: case HtmlTag::H3:
                case HtmlTag::H4: case HtmlTag::H5: case HtmlTag::H6:
                    flushLine();
                    popFmt();
                    needSpacing = true;
                    break;
                case HtmlTag::P:
                    flushLine();
                    needSpacing = true;
                    break;
                case HtmlTag::A:
                    // Record link region from unflushed line before clearing inLink
                    if (inLink && curLineLen > linkStartCol && lnkCount < maxLinks) {
                        ClickableLink* lk = &links[lnkCount];
                        lk->lineIndex = lineCount;
                        lk->startCol = indent + linkStartCol;
                        lk->endCol = indent + curLineLen;
                        scpy(lk->href, linkHref, HTML_MAX_ATTR_LENGTH);
                        lnkCount++;
                    }
                    inLink = false;
                    popFmt();
                    break;
                case HtmlTag::B: case HtmlTag::Strong:
                    bold = false;
                    break;
                case HtmlTag::I: case HtmlTag::Em:
                    italic = false;
                    break;
                case HtmlTag::U:
                    underline = false;
                    break;
                case HtmlTag::Ul: case HtmlTag::Ol:
                    flushLine();
                    if (listDepth > 0) listDepth--;
                    indent = listDepth * 2;
                    orderedCount = 0;
                    break;
                case HtmlTag::Li:
                    flushLine();
                    break;
                case HtmlTag::Pre:
                    flushLine();
                    inPre = false;
                    colorR = 0; colorG = 0; colorB = 0;
                    break;
                case HtmlTag::Code:
                    if (!inPre) {
                        popFmt();
                    }
                    break;
                case HtmlTag::Blockquote:
                    flushLine();
                    if (indent >= 4) indent -= 4;
                    else indent = 0;
                    break;
                case HtmlTag::Div:
                    flushLine();
                    break;
                case HtmlTag::Th:
                    bold = false;
                    break;
                case HtmlTag::Table: case HtmlTag::Tr:
                    flushLine();
                    break;
                default:
                    break;
            }
            continue;
        }

        // Opening tags
        switch (elem->tag) {
            case HtmlTag::Head:
                inHead = true;
                break;
            case HtmlTag::Title:
                inTitle = true;
                break;
            case HtmlTag::Html:
                break;
            case HtmlTag::Body:
                inHead = false;
                break;
            case HtmlTag::H1: case HtmlTag::H2: case HtmlTag::H3:
            case HtmlTag::H4: case HtmlTag::H5: case HtmlTag::H6:
                inHead = false;
                flushLine();
                pushFmt();
                setHeadingStyle(elem->tag);
                break;
            case HtmlTag::P:
                inHead = false;
                flushLine();
                needSpacing = true;
                break;
            case HtmlTag::Br:
                flushLine(true);
                break;
            case HtmlTag::Hr:
                inHead = false;
                emitHr();
                break;
            case HtmlTag::A:
                pushFmt();
                inLink = true;
                linkStartCol = curLineLen;
                scpy(linkHref, elem->href, HTML_MAX_ATTR_LENGTH);
                colorR = 100; colorG = 150; colorB = 255;
                underline = true;
                break;
            case HtmlTag::B: case HtmlTag::Strong:
                bold = true;
                break;
            case HtmlTag::I: case HtmlTag::Em:
                italic = true;
                break;
            case HtmlTag::U:
                underline = true;
                break;
            case HtmlTag::Ul:
                inHead = false;
                flushLine();
                listDepth++;
                orderedCount = 0;
                indent = listDepth * 2;
                break;
            case HtmlTag::Ol:
                inHead = false;
                flushLine();
                listDepth++;
                indent = listDepth * 2;
                orderedCount = 1;
                break;
            case HtmlTag::Li: {
                inHead = false;
                flushLine();
                if (orderedCount > 0) {
                    orderedCount++;
                    // Number prefix
                    char num[8];
                    u32 n = orderedCount;
                    u32 ni = 0;
                    if (n >= 10) { num[ni++] = '0' + static_cast<char>(n / 10); }
                    num[ni++] = '0' + static_cast<char>(n % 10);
                    num[ni++] = '.';
                    num[ni++] = ' ';
                    num[ni] = '\0';
                    addWord(num, ni);
                } else {
                    addWord("-", 1);
                    addWord("", 0); // just separator
                }
                break;
            }
            case HtmlTag::Pre:
                inHead = false;
                flushLine();
                inPre = true;
                colorR = 50; colorG = 50; colorB = 50;
                break;
            case HtmlTag::Code:
                if (!inPre) {
                    pushFmt();
                    colorR = 180; colorG = 60; colorB = 60;
                }
                break;
            case HtmlTag::Blockquote:
                inHead = false;
                flushLine();
                indent += 4;
                needSpacing = true;
                break;
            case HtmlTag::Img:
                if (elem->alt[0]) {
                    addWord("[", 1);
                    addText(elem->alt, slen(elem->alt));
                    addWord("]", 1);
                } else {
                    addWord("[Image]", 7);
                }
                break;
            case HtmlTag::Div:
                inHead = false;
                flushLine();
                break;
            case HtmlTag::Table:
                inHead = false;
                flushLine();
                break;
            case HtmlTag::Tr:
                inHead = false;
                flushLine();
                break;
            case HtmlTag::Td: case HtmlTag::Th:
                if (curLineLen > 0) addWord("|", 1);
                if (elem->tag == HtmlTag::Th) bold = true;
                break;
            default:
                break;
        }
    }

    // Flush remaining
    flushLine();

    if (linkCount) *linkCount = lnkCount;
    return lineCount;
}

}
