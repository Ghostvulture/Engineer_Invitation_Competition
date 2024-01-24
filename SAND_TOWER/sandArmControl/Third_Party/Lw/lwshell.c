/**
 * \file            lwshell.c
 * \brief           Lightweight shell
 */

/*
 * Copyright (c) 2023 Tilen MAJERLE
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE
 * AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * This file is part of LwSHELL - Lightweight shell library.
 *
 * Author:          Tilen MAJERLE <tilen@majerle.eu>
 * Version:         v1.2.0
 */
#include <string.h>
#include "lwshell.h"

/* Check enabled features */
#if LWSHELL_CFG_USE_LIST_CMD && !LWSHELL_CFG_USE_OUTPUT
#error "To use list command feature, LWSHELL_CFG_USE_OUTPUT must be enabled"
#endif /* LWSHELL_CFG_USE_LIST_CMD && !LWSHELL_CFG_USE_OUTPUT */
#if !LWSHELL_CFG_USE_DYNAMIC_COMMANDS && !LWSHELL_CFG_USE_STATIC_COMMANDS
#error "At least one of LWSHELL_CFG_USE_DYNAMIC_COMMANDS or !LWSHELL_CFG_USE_STATIC_COMMANDS must be enabled"
#endif /* !LWSHELL_CFG_USE_DYNAMIC_COMMANDS && !LWSHELL_CFG_USE_STATIC_COMMANDS */

/* Default characters */
#define LWSHELL_ASCII_NULL      0x00 /*!< Null character */
#define LWSHELL_ASCII_BACKSPACE 0x08 /*!< Backspace */
#define LWSHELL_ASCII_LF        0x0A /*!< Line feed */
#define LWSHELL_ASCII_CR        0x0D /*!< Carriage return */
#define LWSHELL_ASCII_DEL       0x7F /*!< Delete character */
#define LWSHELL_ASCII_SPACE     0x20 /*!< Space character */

#if LWSHELL_CFG_USE_OUTPUT
#define LWSHELL_OUTPUT(lwobj, str)                                                                                     \
    do {                                                                                                               \
        if ((lwobj)->out_fn != NULL && (str) != NULL) {                                                                \
            (lwobj)->out_fn((str), (lwobj));                                                                           \
        }                                                                                                              \
    } while (0)
#else
#define LWSHELL_OUTPUT(lwobj, str)
#endif

/* Array of all commands */
static lwshell_t shell;

/* Get shell instance from input */
#define LWSHELL_GET_LWOBJ(lwobj) ((lwobj) != NULL ? (lwobj) : (&shell))

/* Add character to instance */
#define LWSHELL_ADD_CH(lwobj, ch)                                                                                      \
    do {                                                                                                               \
        if ((lwobj)->buff_ptr < (LWSHELL_ARRAYSIZE((lwobj)->buff) - 1)) {                                              \
            (lwobj)->buff[(lwobj)->buff_ptr] = ch;                                                                     \
            (lwobj)->buff[++(lwobj)->buff_ptr] = '\0';                                                                 \
        }                                                                                                              \
    } while (0)

/* Reset buffers */
#define LWSHELL_RESET_BUFF(lwobj)                                                                                      \
    do {                                                                                                               \
        memset((lwobj)->buff, 0x00, sizeof((lwobj)->buff));                                                            \
        memset((lwobj)->argv, 0x00, sizeof((lwobj)->argv));                                                            \
        (lwobj)->buff_ptr = 0;                                                                                         \
    } while (0)

/**
 * \brief           Parse input string
 * \param[in]       lwobj: LwSHELL instance
 */
static void
prv_parse_input(lwshell_t* lwobj) {
    size_t s_len;
    char* str;

    /* Check string length and compare with buffer pointer */
    s_len = strlen(lwobj->buff);
    if (s_len != lwobj->buff_ptr) {
        return;
    }

    /* Must be more than `1` character since we have to include end of line */
    if (lwobj->buff_ptr > 0) {
        /* Set default values */
        lwobj->argc = 0;
        lwobj->argv[0] = lwobj->buff;

        /* Process complete input */
        str = lwobj->buff;

        /* Process complete string */
        while (*str != '\0') {
            while (*str == ' ' && ++str) {} /* Remove leading spaces */
            if (*str == '\0') {
                break;
            }

            /* Check if it starts with quote to handle escapes */
            if (*str == '"') {
                ++str;
                lwobj->argv[lwobj->argc++] = str; /* Set start of argument after quotes */

                /* Process until end of quote */
                while (*str != '\0') {
                    if (*str == '\\') {
                        ++str;
                        if (*str == '"') {
                            ++str;
                        }
                    } else if (*str == '"') {
                        *str = '\0';
                        ++str;
                        break;
                    } else {
                        ++str;
                    }
                }
            } else {
                lwobj->argv[lwobj->argc++] = str; /* Set start of argument directly on character */
                while (*str != ' ' && *str != '\0') {
                    if (*str == '"') { /* Quote should not be here... */
                        *str = '\0';   /* ...add NULL termination to end token */
                    }
                    ++str;
                }
                if (*str == '\0') {
                    break;
                }
                *str = '\0';
                ++str;
            }

            /* Check for number of arguments */
            if (lwobj->argc == LWSHELL_ARRAYSIZE(lwobj->argv)) {
                break;
            }
        }

        /* Check for command */
        if (lwobj->argc > 0) {
            const lwshell_cmd_t* c = NULL;
            size_t arg_len = strlen(lwobj->argv[0]);

#if LWSHELL_CFG_USE_DYNAMIC_COMMANDS
            /* Process all dynamic commands */
            if (c == NULL && lwobj->dynamic_cmds_cnt > 0) {
                for (size_t i = 0; i < lwobj->dynamic_cmds_cnt; ++i) {
                    if (arg_len == strlen(lwobj->dynamic_cmds[i].name)
                        && strncmp(lwobj->dynamic_cmds[i].name, lwobj->argv[0], arg_len) == 0) {
                        c = &lwobj->dynamic_cmds[i];
                        break;
                    }
                }
            }
#endif /* LWSHELL_CFG_USE_DYNAMIC_COMMANDS */

#if LWSHELL_CFG_USE_STATIC_COMMANDS
            /* Process all static commands */
            if (c == NULL && lwobj->static_cmds != NULL && lwobj->static_cmds_cnt > 0) {
                for (size_t i = 0; i < lwobj->static_cmds_cnt; ++i) {
                    if (arg_len == strlen(lwobj->static_cmds[i].name)
                        && strncmp(lwobj->static_cmds[i].name, lwobj->argv[0], arg_len) == 0) {
                        c = &lwobj->static_cmds[i];
                        break;
                    }
                }
            }
#endif /* LWSHELL_CFG_USE_STATIC_COMMANDS */

            /* Valid command ready? */
            if (c != NULL) {
                if (lwobj->argc == 2 && lwobj->argv[1][0] == '-' && lwobj->argv[1][1] == 'h'
                    && lwobj->argv[1][2] == '\0') {
                    /* Here we can print version */
                    LWSHELL_OUTPUT(lwobj, c->desc);
                    LWSHELL_OUTPUT(lwobj, "\r\n");
                } else {
                    c->fn(lwobj->argc, lwobj->argv);
                }
#if LWSHELL_CFG_USE_LIST_CMD
            } else if (strncmp(lwobj->argv[0], "listcmd", 7) == 0) {
                LWSHELL_OUTPUT(lwobj, "List of registered commands\r\n");
#if LWSHELL_CFG_USE_DYNAMIC_COMMANDS
                for (size_t i = 0; i < lwobj->dynamic_cmds_cnt; ++i) {
                    LWSHELL_OUTPUT(lwobj, lwobj->dynamic_cmds[i].name);
                    LWSHELL_OUTPUT(lwobj, "\t\t\t");
                    LWSHELL_OUTPUT(lwobj, lwobj->dynamic_cmds[i].desc);
                    LWSHELL_OUTPUT(lwobj, "\r\n");
                }
#endif /* LWSHELL_CFG_USE_DYNAMIC_COMMANDS */
#if LWSHELL_CFG_USE_STATIC_COMMANDS
                for (size_t i = 0; i < lwobj->static_cmds_cnt; ++i) {
                    LWSHELL_OUTPUT(lwobj, lwobj->static_cmds[i].name);
                    LWSHELL_OUTPUT(lwobj, "\t\t\t");
                    LWSHELL_OUTPUT(lwobj, lwobj->static_cmds[i].desc);
                    LWSHELL_OUTPUT(lwobj, "\r\n");
                }
#endif /* LWSHELL_CFG_USE_STATIC_COMMANDS */
#endif /* LWSHELL_CFG_USE_LIST_CMD */
            } else {
                LWSHELL_OUTPUT(lwobj, "Unknown command\r\n");
            }
        }
    }
}

/**
 * \brief           Initialize shell interface
 * \param[in]       lwobj: LwSHELL object instance. Set to `NULL` to use default one
 * \return          \ref lwshellOK on success, member of \ref lwshellr_t otherwise
 */
lwshellr_t
lwshell_init_ex(lwshell_t* lwobj) {
    lwobj = LWSHELL_GET_LWOBJ(NULL);
    memset(lwobj, 0x00, sizeof(*lwobj));
    return lwshellOK;
}

#if LWSHELL_CFG_USE_OUTPUT || __DOXYGEN__

/**
 * \brief           Set output function to use to print data from library to user
 * \param[in]       lwobj: LwSHELL object instance. Set to `NULL` to use default one
 * \param[in]       out_fn: Output function to print library data.
 *                      Set to `NULL` to disable the feature
 * \return          \ref lwshellOK on success, member of \ref lwshellr_t otherwise
 */
lwshellr_t
lwshell_set_output_fn_ex(lwshell_t* lwobj, lwshell_output_fn out_fn) {
    lwobj = LWSHELL_GET_LWOBJ(lwobj);
    lwobj->out_fn = out_fn;
    return lwshellOK;
}

#endif /* LWSHELL_CFG_USE_OUTPUT || __DOXYGEN__ */

#if LWSHELL_CFG_USE_DYNAMIC_COMMANDS || __DOXYGEN__

/**
 * \brief           Register new command to shell
 * \param[in]       lwobj: LwSHELL object instance. Set to `NULL` to use default one
 * \param[in]       cmd_name: Command name. This one is used when entering shell command
 * \param[in]       cmd_fn: Function to call on command match
 * \param[in]       desc: Custom command description
 * \return          \ref lwshellOK on success, member of \ref lwshellr_t otherwise
 * \note            Available only when \ref LWSHELL_CFG_USE_DYNAMIC_COMMANDS is enabled
 */
lwshellr_t
lwshell_register_cmd_ex(lwshell_t* lwobj, const char* cmd_name, lwshell_cmd_fn cmd_fn, const char* desc) {
    lwobj = LWSHELL_GET_LWOBJ(lwobj);

    if (cmd_name == NULL || cmd_fn == NULL || strlen(cmd_name) == 0) {
        return lwshellERRPAR;
    }

    /* Check for memory available */
    if (lwobj->dynamic_cmds_cnt < LWSHELL_ARRAYSIZE(lwobj->dynamic_cmds)) {
        lwobj->dynamic_cmds[lwobj->dynamic_cmds_cnt].name = cmd_name;
        lwobj->dynamic_cmds[lwobj->dynamic_cmds_cnt].fn = cmd_fn;
        lwobj->dynamic_cmds[lwobj->dynamic_cmds_cnt].desc = desc;

        ++lwobj->dynamic_cmds_cnt;
        return lwshellOK;
    }
    return lwshellERRMEM;
}

#endif /* LWSHELL_CFG_USE_DYNAMIC_COMMANDS || __DOXYGEN__ */

#if LWSHELL_CFG_USE_STATIC_COMMANDS || __DOXYGEN__

/**
 * \brief           Register new command to shell
 * \param[in]       lwobj: LwSHELL object instance. Set to `NULL` to use default one
 * \param[in]       cmds: Array of const static commands. It can be from non-volatile memory.
 * \param[in]       cmds_len: Length of array elements
 * \return          \ref lwshellOK on success, member of \ref lwshellr_t otherwise
 * \note            Available only when \ref LWSHELL_CFG_USE_STATIC_COMMANDS is enabled
 */
lwshellr_t
lwshell_register_static_cmds_ex(lwshell_t* lwobj, const lwshell_cmd_t* cmds, size_t cmds_len) {
    lwobj = LWSHELL_GET_LWOBJ(lwobj);
    lwobj->static_cmds = cmds;
    lwobj->static_cmds_cnt = cmds_len;
    return lwshellOK;
}

#endif /* LWSHELL_CFG_USE_STATIC_COMMANDS || __DOXYGEN__ */

/**
 * \brief           Input data to shell processing
 * \param[in]       lwobj: LwSHELL object instance. Set to `NULL` to use default one
 * \param[in]       in_data: Input data to process
 * \param[in]       len: Length of data for input
 * \return          \ref lwshellOK on success, member of \ref lwshellr_t otherwise
 */
lwshellr_t
lwshell_input_ex(lwshell_t* lwobj, const void* in_data, size_t len) {
    const char* d = in_data;
    lwobj = LWSHELL_GET_LWOBJ(lwobj);

    if (in_data == NULL || len == 0) {
        return lwshellERRPAR;
    }

    /* Process all bytes */
    for (size_t i = 0; i < len; ++i) {
        switch (d[i]) {
            case LWSHELL_ASCII_CR: {
                LWSHELL_OUTPUT(lwobj, "\r");
                prv_parse_input(lwobj);
                LWSHELL_RESET_BUFF(lwobj);
                break;
            }
            case LWSHELL_ASCII_LF: {
                LWSHELL_OUTPUT(lwobj, "\n");
                prv_parse_input(lwobj);
                LWSHELL_RESET_BUFF(lwobj);
                break;
            }
            case LWSHELL_ASCII_BACKSPACE: {
                /* Try to delete character from buffer */
                if (lwobj->buff_ptr > 0) {
                    --lwobj->buff_ptr;
                    lwobj->buff[lwobj->buff_ptr] = '\0';
                    LWSHELL_OUTPUT(lwobj, "\b \b");
                }
                break;
            }
            default: {
                char str[2] = {d[i]};
                LWSHELL_OUTPUT(lwobj, str);
                if (d[i] >= 0x20 && d[i] < 0x7F) {
                    LWSHELL_ADD_CH(lwobj, d[i]);
                }
            }
        }
    }
    return lwshellOK;
}
