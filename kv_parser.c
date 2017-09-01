#include "kv_parser.h"
#include "stdio.h"

#define EMIT_DATA_CB(FOR, ptr, len)                                 \
do {                                                                \
	if (settings->on_##FOR) {                                       \
		if (settings->on_##FOR(parser, ptr, len) != 0) {            \
			return i;                                               \
		}                                                           \
	}                                                               \
} while (0)

#define EMIT_NOTIFY_CB(FOR)                                         \
do {                                                                \
	if (settings->on_##FOR) {                                       \
		if (settings->on_##FOR(parser) != 0) {                      \
			return i;                                               \
		}                                                           \
	}                                                               \
} while (0)

// {ws*}{w1?}key{w2?}{ws*}{s1}{ws*}{w1?}val{w2?}{ws*}{s2}
enum kv_parser_status {
	KV_STATUS_BEFORE_KEY_WHITESPACE,
	KV_STATUS_BEFORE_KEY_WRAPPER,
	KV_STATUS_KEY,
	KV_STATUS_AFTER_KEY_WRAPPER,
	KV_STATUS_AFTER_KEY_WHITESPACE,
	KV_STATUS_SEPERATOR_1,
	KV_STATUS_BEFORE_VAL_WHITESPACE,
	KV_STATUS_BEFORE_VAL_WRAPPER,
	KV_STATUS_VAL,
	KV_STATUS_AFTER_VAL_WRAPPER,
	KV_STATUS_AFTER_VAL_WHITESPACE,
	KV_STATUS_SEPERATOR_2,
};

void kv_parser_init(kv_parser* parser, kv_parser_settings* settings) {
	parser->status = KV_STATUS_BEFORE_KEY_WHITESPACE;
}
size_t kv_parser_execute(kv_parser* parser, kv_parser_settings* settings, const char* data, size_t size) {
	size_t i = 0, mark = 0;
	while(i<size) {
		char c = data[i];
		int  last = (i == size - 1);
		switch(parser->status) {
		case KV_STATUS_BEFORE_KEY_WHITESPACE:
			if(settings->w1 != '\0' && c == settings->w1) {
				parser->status = KV_STATUS_BEFORE_KEY_WRAPPER;
				goto CONTINUE_REDO;
			}else if(!isspace(c)) {
				mark = i;
				parser->status = KV_STATUS_KEY;
				goto CONTINUE_REDO;
			}
			break;
		case KV_STATUS_BEFORE_KEY_WRAPPER:
			if(c != settings->w1) {
				mark = i;
				parser->status = KV_STATUS_KEY;
				goto CONTINUE_REDO;
			}
			break;
		case KV_STATUS_KEY:
			if(settings->w2 != '\0' && c == settings->w2) {
				parser->status = KV_STATUS_AFTER_KEY_WRAPPER;
				EMIT_DATA_CB(key, data + mark, i - mark);
				EMIT_NOTIFY_CB(key_end);
				goto CONTINUE_REDO;
			}else if(isspace(c)) {
				parser->status = KV_STATUS_AFTER_KEY_WHITESPACE;
				EMIT_DATA_CB(key, data + mark, i - mark);
				EMIT_NOTIFY_CB(key_end);
				goto CONTINUE_REDO;
			}else if(c == settings->s1) {
				parser->status = KV_STATUS_SEPERATOR_1;
				EMIT_DATA_CB(key, data + mark, i - mark);
				EMIT_NOTIFY_CB(key_end);
				goto CONTINUE_REDO;
			}
			if(last) {
				EMIT_DATA_CB(key, data + mark, i - mark + 1);
			}
			break;
		case KV_STATUS_AFTER_KEY_WRAPPER:
			if(c != settings->w2) {
				parser->status = KV_STATUS_AFTER_KEY_WHITESPACE;
				goto CONTINUE_REDO;
			}
			break;
		case KV_STATUS_AFTER_KEY_WHITESPACE:
			if(c == settings->s1) {
				parser->status = KV_STATUS_SEPERATOR_1;
				goto CONTINUE_REDO;
			}else if(!isspace(c)) {
				goto CONTINUE_STOP; // 错误：非空白且非分隔符
			}
			break;
		case KV_STATUS_SEPERATOR_1:
			if(c != settings->s1) {
				parser->status = KV_STATUS_BEFORE_VAL_WHITESPACE;
				goto CONTINUE_REDO;
			}
			break;
		case KV_STATUS_BEFORE_VAL_WHITESPACE:
			if(settings->w1 != '\0' && c == settings->w1) {
				parser->status = KV_STATUS_BEFORE_VAL_WRAPPER;
				goto CONTINUE_REDO;
			}else if(!isspace(c)) {
				mark = i;
				parser->status = KV_STATUS_VAL;
				goto CONTINUE_REDO;
			}
			break;
		case KV_STATUS_BEFORE_VAL_WRAPPER:
			if(c != settings->w1) {
				parser->status = KV_STATUS_VAL;
				mark = i;
				goto CONTINUE_REDO;
			}
			break;
		case KV_STATUS_VAL:
			if(settings->w2 != '\0' && c == settings->w2) {
				parser->status = KV_STATUS_AFTER_VAL_WRAPPER;
				EMIT_DATA_CB(val, data + mark, i - mark);
				EMIT_NOTIFY_CB(val_end);
				goto CONTINUE_REDO;
			}else if(isspace(c)) {
				parser->status = KV_STATUS_AFTER_VAL_WHITESPACE;
				EMIT_DATA_CB(val, data + mark, i - mark);
				EMIT_NOTIFY_CB(val_end);
				goto CONTINUE_REDO;
			}else if(c == settings->s2) {
				parser->status = KV_STATUS_SEPERATOR_2;
				EMIT_DATA_CB(val, data + mark, i - mark);
				EMIT_NOTIFY_CB(val_end);
				goto CONTINUE_REDO;
			}
			if(last) {
				EMIT_DATA_CB(val, data + mark, i - mark + 1);
			}
			break;
		case KV_STATUS_AFTER_VAL_WRAPPER:
			if(c != settings->w2) {
				parser->status = KV_STATUS_AFTER_VAL_WHITESPACE;
				goto CONTINUE_REDO;
			}
			break;
		case KV_STATUS_AFTER_VAL_WHITESPACE:
			if(c == settings->s2) {
				parser->status = KV_STATUS_SEPERATOR_2;
				goto CONTINUE_REDO;
			}else if(!isspace(c)) {
				goto CONTINUE_STOP; // 错误：非空白且非分隔符
			}
			break;
		case KV_STATUS_SEPERATOR_2:
			if(c != settings->s2) {
				parser->status = KV_STATUS_BEFORE_KEY_WHITESPACE;
				goto CONTINUE_REDO;
			}
			break;
		}
CONTINUE_NEXT:
		++i;
CONTINUE_REDO:
		continue;
CONTINUE_STOP:
		break;
	}
}