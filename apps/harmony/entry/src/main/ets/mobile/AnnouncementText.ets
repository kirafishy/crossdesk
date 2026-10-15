export interface AnnouncementSpan { text: string; url: string; }

// Same restricted plain-text/link grammar as Android AnnouncementText.java.
export function isAnnouncementURL(address: string): boolean {
  return !/[\x00-\x20\x7f\\<>\"]/.test(address) &&
    /^https?:\/\/(?:[^\/?#]*@)?(?:\[[0-9a-f:.]+\]|[^\/?#:@\[\]]+)(?::[0-9]+)?(?:[\/?#]|$)/i.test(address);
}
function hasScheme(text: string, start: number): boolean {
  return /^https?:\/\//i.test(text.substring(start, start + 8));
}
function urlEnd(text: string, start: number): number {
  let parentheses = 0, brackets = 0, end = start;
  for (; end < text.length; end++) {
    const c = text.charAt(end);
    if (c.charCodeAt(0) <= 0x20 || c.charCodeAt(0) === 0x7f || c === '\\' || c === '`' ||
        '<>\"\'{}，。；：！？、（）【】《》「」『』“”‘’'.includes(c)) break;
    if (c === '(') parentheses++;
    if (c === ')') { if (parentheses === 0) break; parentheses--; }
    if (c === '[') brackets++;
    if (c === ']') { if (brackets === 0) break; brackets--; }
  }
  return end;
}
export function announcementSpans(body: string): AnnouncementSpan[] {
  const text = body.replace(/\r\n/g, '\n'), result: AnnouncementSpan[] = [];
  let plain = '';
  for (let position = 0; position < text.length;) {
    let label = '', address = '', next = position;
    if (text.charAt(position) === '[') {
      const labelEnd = text.indexOf(']', position + 1), start = labelEnd + 2;
      if (labelEnd > position + 1 && start < text.length && text.charAt(labelEnd + 1) === '(' &&
          !/[\[\]\r\n]/.test(text.substring(position + 1, labelEnd)) && hasScheme(text, start)) {
        const end = urlEnd(text, start), url = text.substring(start, end);
        if (text.charAt(end) === ')' && isAnnouncementURL(url)) {
          label = text.substring(position + 1, labelEnd); address = url; next = end + 1;
        }
      }
    }
    if (!address && hasScheme(text, position)) {
      let end = urlEnd(text, position);
      while (end > position && '.,;:!?'.includes(text.charAt(end - 1))) end--;
      const url = text.substring(position, end);
      if (isAnnouncementURL(url)) { label = address = url; next = end; }
    }
    if (address) {
      if (plain) { result.push({ text: plain, url: '' }); plain = ''; }
      result.push({ text: label, url: address }); position = next;
    } else plain += text.charAt(position++);
  }
  if (plain) result.push({ text: plain, url: '' });
  return result;
}

export function announcementByteLength(text: string): number {
  let bytes = 0;
  for (let i = 0; i < text.length; i++) {
    const c = text.charCodeAt(i);
    if (c < 0x80) bytes++;
    else if (c < 0x800) bytes += 2;
    else if (c >= 0xd800 && c <= 0xdbff && i + 1 < text.length &&
        text.charCodeAt(i + 1) >= 0xdc00 && text.charCodeAt(i + 1) <= 0xdfff) { bytes += 4; i++; }
    else bytes += c >= 0xd800 && c <= 0xdfff ? 1 : 3; // Java UTF-8 replaces lone surrogates with '?'.
  }
  return bytes;
}
