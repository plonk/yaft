void reset_func(void (**func)(terminal *term, void *arg), int num)
{
	int i;

	for (i = 0; i < num; i++)
		func[i] = ignore;
}

void load_ctrl_func(void (**func)(terminal *term, void *arg), int num)
{
	reset_func(func, num);

	func[0x08] = bs;        /* BS  */
	func[0x09] = tab;       /* HT  */
	func[0x0A] = nl;        /* LF  */
	func[0x0B] = nl;        /* VT  */
	func[0x0C] = nl;        /* FF  */
	func[0x0D] = cr;        /* CR  */
	func[0x1B] = enter_esc; /* ESC */
}

void load_esc_func(void (**func)(terminal *term, void *arg), int num)
{
	reset_func(func, num);

	func['7'] = save_state;
	func['8'] = restore_state;
	func['D'] = nl;
	func['E'] = crnl;
	func['H'] = set_tabstop;
	func['M'] = reverse_nl;
	func['Z'] = identify;
	func['['] = enter_csi;
	func[']'] = enter_osc;
	func['c'] = ris;
}

void load_csi_func(void (**func)(terminal *term, void *arg), int num)
{
	reset_func(func, num);

	func['@'] = insert_blank;
	func['A'] = curs_up;
	func['B'] = curs_down;
	func['C'] = curs_forward;
	func['D'] = curs_back;
	func['E'] = curs_nl;
	func['F'] = curs_pl;
	func['G'] = curs_col;
	func['H'] = curs_pos;
	func['J'] = erase_display;
	func['K'] = erase_line;
	func['L'] = insert_line;
	func['M'] = delete_line;
	func['P'] = delete_char;
	func['X'] = erase_char;
	func['a'] = curs_forward;
	func['c'] = identify;
	func['d'] = curs_line;
	func['e'] = curs_down;
	func['f'] = curs_pos;
	func['g'] = clear_tabstop;
	func['h'] = set_mode;
	func['l'] = reset_mode;
	func['m'] = set_attr;
	func['n'] = status_report;
	func['r'] = set_margin;
	func['s'] = save_state;
	func['u'] = restore_state;
	func['`'] = curs_col;
}

void load_osc_func(void (**func)(terminal *term, void *arg), int num)
{
	reset_func(func, num);

	func[4] = set_palette;
	func[104] = reset_palette;
}

/* need to rewrite this function */
int parse_osc(terminal *term, int *argc, char **argv)
{
	int i, length;
	u8 c, *cp;
	escape *ep;

	ep = &term->esc;

	length = strlen(ep->buf);
	c = ep->buf[length - 1];

	ep->buf[length - 1] = '\0';
	if (c == 0x5C)
		ep->buf[length - 2] = '\0';

	cp = ep->buf;
	while (cp != &ep->buf[length - 1]) {
		if (*cp == ';' || *cp == ':' || *cp == '/' || *cp == '#')
			*cp = '\0';
		cp++;
	}

	cp = ep->buf;
start:
	if (isalnum(*cp)) {
		argv[*argc] = cp;
		*argc += 1;
	}

	while (isalnum(*cp))
		cp++;

	while (!isalnum(*cp) && cp != &ep->buf[length - 1])
		cp++;

	if (cp != &ep->buf[length - 1])
		goto start;

	return atoi(argv[0]);
}

/* need to rewrite this function */
u8 parse_csi(terminal *term, int *argc, char **argv)
{
	int i, length;
	u8 c, *cp;
	escape *ep;

	ep = &term->esc;

	length = strlen(ep->buf);
	c = ep->buf[length - 1];
	ep->buf[length - 1] = '\0';

	cp = ep->buf;
	while (cp != &ep->buf[length - 1]) {
		if (*cp == ';')
			*cp = '\0';
		cp++;
	}

	cp = ep->buf;
start:
	if (isdigit(*cp)) {
		argv[*argc] = cp;
		*argc += 1;
	}

	while (isdigit(*cp))
		cp++;

	while (!isdigit(*cp) && cp != &ep->buf[length - 1])
			cp++;

	if (cp != &ep->buf[length - 1])
		goto start;

	return c;
}

static char *ctrl_char[] = {
	"NUL","SOH","STX","ETX","EOT","ENQ","ACK","BEL",
	"BS ","HT ","LF ","VT ","FF ","CR ","SO ","SI ",
	"DLE","DC1","DC2","DC3","DC4","NAK","SYN","ETB",
	"CAN","EM ","SUB","ESC","FS ","GS ","RS ","US ",
};

void control_character(terminal *term, u8 ch)
{
	if (DEBUG)
		fprintf(stderr, "control char: %s\n", ctrl_char[ch]);

	ctrl_func[ch](term, NULL);
}

void esc_sequence(terminal *term, u8 ch)
{
	if (DEBUG)
		fprintf(stderr, "escape sequence char: ESC %c\n", ch);

	esc_func[ch](term, NULL);

	if (ch != '[' && ch != ']')
		term->esc.state = RESET;
}

void reset_parm(parm_t *pt)
{
	int i;

	pt->argc = 0;
	for (i = 0; i < ESC_PARAMS; i++)
		pt->argv[i] = NULL;
}

void csi_sequence(terminal *term)
{
 	u8 c;
	parm_t parm;

	if (DEBUG)
		fprintf(stderr, "escape sequence char: CSI %s\n", term->esc.buf);

	reset_parm(&parm);
	c = parse_csi(term, &parm.argc, parm.argv);

	csi_func[c](term, &parm);
	reset_esc(term);
}

void osc_sequence(terminal *term)
{
	int num;
	parm_t parm;

	if (DEBUG)
		fprintf(stderr, "escape sequence char: OSC %s\n", term->esc.buf);

	reset_parm(&parm);
	num = parse_osc(term, &parm.argc, parm.argv);

	osc_func[num](term, &parm);
	reset_esc(term);
}

void utf8_character(terminal *term, u8 ch)
{
	if (0xC2 <= ch && ch <= 0xDF) {
		term->ucs.length = 1;
		term->ucs.count = 0;
		term->ucs.code = ch & 0x1F;
		return;
	}
	else if (0xE0 <= ch && ch <= 0xEF) {
		term->ucs.length = 2;
		term->ucs.count = 0;
		term->ucs.code = ch & 0x0F;
		return;
	}
	else if (0xF0 <= ch && ch <= 0xF4) {
		term->ucs.length = 3;
		term->ucs.count = 0;
		term->ucs.code = ch & 0x07;
		return;
	}
	else if (0x80 <= ch && ch <= 0xBF) {
		if (term->ucs.length == 0) {
			reset_ucs(term);
			return;
		}
		term->ucs.code <<= 6;
		term->ucs.code += ch & 0x3F;
		term->ucs.count++;
	}
	else { /* 0xC0 - 0xC1, 0xF5 - 0xFF: illegal byte */
		reset_ucs(term);
		return;
	}

	if (term->ucs.count >= term->ucs.length) {
		if (DEBUG)
			fprintf(stderr, "unicode: U+%4.4X\n", ch);
		addch(term, term->ucs.code);
		reset_ucs(term);
	}
}

void parse(terminal *term, u8 *buf, int size)
{
	u8 ch;
	int i;

	for (i = 0; i < size; i++) {
		ch = buf[i];
		if (term->esc.state == RESET) {
			if (ch <= 0x1F)
				control_character(term, ch);
			else if (ch <= 0x7F) {
				if (DEBUG)
					fprintf(stderr, "ascii: %c\n", ch);
				addch(term, ch);
			}
			else if (ch <= 0xFF)
				utf8_character(term, ch);
		}
		else if (term->esc.state == ESC) {
			if (ch <= 0x7F)
				esc_sequence(term, ch);
			else
				term->esc.state = RESET;
		}
		else if (term->esc.state == CSI) {
			if (push_esc(term, ch))
				csi_sequence(term);
		}
		else if (term->esc.state == OSC) {
			if (push_esc(term, ch))
				osc_sequence(term);
		}
	}
}
