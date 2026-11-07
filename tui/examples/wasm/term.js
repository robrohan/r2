/*
    The terminal this page is: just enough VT100 for what r2_tui sends
    (cursor moves CUP and CUF, SGR colours, ?25 cursor, ?2026 sync, UTF-8
    text), and browser keys turned into the bytes a terminal would send.
    No DOM in here, so it can be tested on its own.
*/

/* the 16 ANSI colours, VGA style (Borland's) */
export const COLOURS = [
    '#000000', '#aa0000', '#00aa00', '#aa5500', '#0000aa', '#aa00aa', '#00aaaa', '#aaaaaa',
    '#555555', '#ff5555', '#55ff55', '#ffff55', '#5555ff', '#ff55ff', '#55ffff', '#ffffff',
];

export const BOLD = 1, UNDERLINE = 2, REVERSE = 4;
export const CONT = -1; /* right half of a wide character */

export class Screen {
    /* width(codepoint) -> 0, 1 or 2 columns: the library's own table */
    constructor(cols, rows, width) {
        this.width = width;
        this.resize(cols, rows);
    }

    resize(cols, rows) {
        this.cols = cols;
        this.rows = rows;
        const n = cols * rows;
        this.cp = new Int32Array(n).fill(32); /* codepoint, CONT for a wide right half */
        this.marks = new Array(n).fill('');   /* combining marks */
        this.style = new Uint16Array(n).fill(7); /* fg | bg << 4 | attr << 8 */
        this.x = 0;
        this.y = 0;
        this.cur = 7;
        this.cursorOn = true;
        this.sync = false;
        this.state = 0;
        this.params = '';
        this.u = 0;
        this.need = 0;
        this.dirty = new Set();
        for (let y = 0; y < rows; y++) this.dirty.add(y);
    }

    write(bytes) {
        for (let i = 0; i < bytes.length; i++) this.byte(bytes[i]);
    }

    byte(b) {
        if (this.state === 1) { /* after ESC */
            this.state = b === 0x5b ? 2 : 0;
            this.params = '';
            if (b === 0x37) this.saved = [this.x, this.y];
            if (b === 0x38 && this.saved) [this.x, this.y] = this.saved;
            return;
        }
        if (this.state === 2) { /* CSI */
            if (b >= 0x30 && b <= 0x3f) this.params += String.fromCharCode(b);
            else if (b >= 0x40 && b <= 0x7e) { this.state = 0; this.csi(String.fromCharCode(b)); }
            return;
        }
        if (this.need) { /* UTF-8 continuation */
            if ((b & 0xc0) === 0x80) {
                this.u = (this.u << 6) | (b & 0x3f);
                if (--this.need === 0) this.print(this.u);
                return;
            }
            this.need = 0;
            this.print(0xfffd);
        }
        if (b === 0x1b) this.state = 1;
        else if (b < 0x20 || b === 0x7f) return; /* r2_tui sends no other controls */
        else if (b < 0x80) this.print(b);
        else if ((b & 0xe0) === 0xc0) { this.u = b & 0x1f; this.need = 1; }
        else if ((b & 0xf0) === 0xe0) { this.u = b & 0x0f; this.need = 2; }
        else if ((b & 0xf8) === 0xf0) { this.u = b & 0x07; this.need = 3; }
        else this.print(0xfffd);
    }

    csi(f) {
        const priv = this.params.startsWith('?');
        const p = (priv ? this.params.slice(1) : this.params).split(';').map((s) => (s === '' ? 0 : parseInt(s, 10)));
        if (priv) {
            const on = f === 'h';
            if (p[0] === 25) { this.cursorOn = on; this.dirty.add(this.y); }
            if (p[0] === 2026) this.sync = on;
            return;
        }
        if (f === 'H') {
            this.dirty.add(this.y);
            this.y = Math.min(Math.max((p[0] || 1) - 1, 0), this.rows - 1);
            this.x = Math.min(Math.max((p[1] || 1) - 1, 0), this.cols - 1);
        } else if (f === 'C') {
            this.x = Math.min(this.x + (p[0] || 1), this.cols - 1);
        } else if (f === 'm') {
            let fg = this.cur & 15, bg = (this.cur >> 4) & 15, a = this.cur >> 8;
            for (const v of p) {
                if (v === 0) { fg = 7; bg = 0; a = 0; }
                else if (v === 1) a |= BOLD;
                else if (v === 4) a |= UNDERLINE;
                else if (v === 7) a |= REVERSE;
                else if (v >= 30 && v <= 37) fg = v - 30;
                else if (v >= 90 && v <= 97) fg = v - 90 + 8;
                else if (v >= 40 && v <= 47) bg = v - 40;
                else if (v >= 100 && v <= 107) bg = v - 100 + 8;
                else if (v === 39) fg = 7;
                else if (v === 49) bg = 0;
            }
            this.cur = fg | (bg << 4) | (a << 8);
        }
    }

    blank(i) {
        this.cp[i] = 32;
        this.marks[i] = '';
        this.style[i] = this.cur;
    }

    print(c) {
        const w = this.width(c);
        if (w === 0) { /* a combining mark joins the cell before */
            if (this.x > 0) this.marks[this.y * this.cols + this.x - 1] += String.fromCodePoint(c);
            return;
        }
        if (this.x + w > this.cols) return; /* r2_tui never wraps */
        const i = this.y * this.cols + this.x;
        /* overwriting half of a wide character blanks the other half, as xterm does */
        if (this.cp[i] === CONT && this.x > 0) this.blank(i - 1);
        const end = i + w - 1;
        if (end + 1 < (this.y + 1) * this.cols && this.cp[end + 1] === CONT) this.blank(end + 1);
        this.cp[i] = c;
        this.marks[i] = '';
        this.style[i] = this.cur;
        if (w === 2) {
            this.cp[i + 1] = CONT;
            this.marks[i + 1] = '';
            this.style[i + 1] = this.cur;
        }
        this.x += w;
        if (this.x >= this.cols) this.x = this.cols - 1;
        this.dirty.add(this.y);
    }

    text(x, y) {
        const i = y * this.cols + x;
        return this.cp[i] === CONT ? '' : String.fromCodePoint(this.cp[i]) + this.marks[i];
    }
}

/* US keyboard: what Shift makes of each key, for Alt (Meta) + key by its code */
const US = {
    Digit1: '1!', Digit2: '2@', Digit3: '3#', Digit4: '4$', Digit5: '5%', Digit6: '6^', Digit7: '7&',
    Digit8: '8*', Digit9: '9(', Digit0: '0)', Minus: '-_', Equal: '=+', BracketLeft: '[{',
    BracketRight: ']}', Backslash: '\\|', Semicolon: ';:', Quote: '\'"', Comma: ',<', Period: '.>',
    Slash: '/?', Backquote: '`~', Space: '  ',
};

/* Ctrl + key by its code */
const CTRL = {
    Space: 0x00, Digit2: 0x00, BracketLeft: 0x1b, Backslash: 0x1c, BracketRight: 0x1d,
    Digit6: 0x1e, Minus: 0x1f, Slash: 0x1f,
};

const FKEYS = { F5: 15, F6: 17, F7: 18, F8: 19, F9: 20, F10: 21, F11: 23, F12: 24 };

/*
    The bytes a terminal sends for a keydown event, or null to leave it
    to the browser. meta: Alt / Option is Meta (ESC + key), so Option
    types Emacs M- keys rather than special characters.
*/
export function keyBytes(e, meta = true) {
    if (e.isComposing || e.metaKey) return null; /* Cmd: the browser's */
    const alt = meta && e.altKey;
    const esc = alt ? '\x1b' : '';
    const m = 1 + (e.shiftKey ? 1 : 0) + (e.altKey ? 2 : 0) + (e.ctrlKey ? 4 : 0);
    const mod = m > 1 ? ';' + m : '';
    const csi = (fin) => '\x1b[' + (mod ? '1' + mod : '') + fin;
    const tilde = (n) => '\x1b[' + n + mod + '~';

    switch (e.key) {
        case 'ArrowUp': return csi('A');
        case 'ArrowDown': return csi('B');
        case 'ArrowRight': return csi('C');
        case 'ArrowLeft': return csi('D');
        case 'Home': return csi('H');
        case 'End': return csi('F');
        case 'PageUp': return tilde(5);
        case 'PageDown': return tilde(6);
        case 'Insert': return tilde(2);
        case 'Delete': return tilde(3);
        case 'Enter': return esc + '\r';
        case 'Tab': return e.shiftKey ? '\x1b[Z' : esc + '\t';
        case 'Backspace': return esc + (e.ctrlKey ? '\x08' : '\x7f');
        case 'Escape': return '\x1b';
        case 'F1': case 'F2': case 'F3': case 'F4': {
            const fin = 'PQRS'[e.key.charCodeAt(1) - 49];
            return mod ? '\x1b[1' + mod + fin : '\x1bO' + fin;
        }
    }
    if (FKEYS[e.key]) return tilde(FKEYS[e.key]);

    /* AltGr on European layouts reports Ctrl+Alt: take the character it makes */
    const ch = [...e.key].length === 1 ? e.key : null;
    if (e.ctrlKey && e.altKey && ch && !e.code.startsWith('Key') && !(e.code in CTRL)) return ch;

    if (e.ctrlKey) {
        if (e.code.startsWith('Key')) return esc + String.fromCharCode(e.code.charCodeAt(3) - 64);
        if (e.code in CTRL) return esc + String.fromCharCode(CTRL[e.code]);
        return null;
    }
    if (alt) {
        /* by the key's code, so Option+f is M-f, not the character macOS makes */
        if (e.code.startsWith('Key')) {
            const c = e.code.charAt(3);
            return '\x1b' + (e.shiftKey ? c : c.toLowerCase());
        }
        if (e.code in US) return '\x1b' + US[e.code][e.shiftKey ? 1 : 0];
        return ch ? '\x1b' + ch : null;
    }
    return ch;
}

/* readable bytes for the key log: \e[A, ^W, ... */
export function showBytes(s) {
    let out = '';
    for (const c of s) {
        const n = c.codePointAt(0);
        if (n === 0x1b) out += '\\e';
        else if (n === 0x7f) out += '^?';
        else if (n < 0x20) out += '^' + String.fromCharCode(n + 64);
        else out += c;
    }
    return out;
}
