/* -*- buffer-read-only: t -*-
 *
 *    keywords.h
 *
 *    Copyright (C) 1994, 1995, 1996, 1997, 1999, 2000, 2001, 2002, 2005,
 *    2006, 2007 by Larry Wall and others
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 * !!!!!!!   DO NOT EDIT THIS FILE   !!!!!!!
 * This file is built by regen/keywords.pl from its data.
 * Any changes made here will be lost!
 */

#define KEY_NULL		0
#define KEY___FILE__		1
#define KEY___LINE__		2
#define KEY___PACKAGE__		3
#define KEY___DATA__		4
#define KEY___END__		5
#define KEY___SUB__		6
#define KEY_AUTOLOAD		7
#define KEY_BEGIN		8
#define KEY_UNITCHECK		9
#define KEY_DESTROY		10
#define KEY_END			11
#define KEY_INIT		12
#define KEY_CHECK		13
#define KEY_abs			14
#define KEY_accept		15
#define KEY_alarm		16
#define KEY_and			17
#define KEY_atan2		18
#define KEY_bind		19
#define KEY_binmode		20
#define KEY_bless		21
#define KEY_break		22
#define KEY_caller		23
#define KEY_chdir		24
#define KEY_chmod		25
#define KEY_chomp		26
#define KEY_chop		27
#define KEY_chown		28
#define KEY_chr			29
#define KEY_chroot		30
#define KEY_close		31
#define KEY_closedir		32
#define KEY_cmp			33
#define KEY_connect		34
#define KEY_continue		35
#define KEY_cos			36
#define KEY_crypt		37
#define KEY_dbmclose		38
#define KEY_dbmopen		39
#define KEY_default		40
#define KEY_defined		41
#define KEY_delete		42
#define KEY_die			43
#define KEY_do			44
#define KEY_dump		45
#define KEY_each		46
#define KEY_else		47
#define KEY_elsif		48
#define KEY_endgrent		49
#define KEY_endhostent		50
#define KEY_endnetent		51
#define KEY_endprotoent		52
#define KEY_endpwent		53
#define KEY_endservent		54
#define KEY_eof			55
#define KEY_eq			56
#define KEY_eval		57
#define KEY_evalbytes		58
#define KEY_exec		59
#define KEY_exists		60
#define KEY_exit		61
#define KEY_exp			62
#define KEY_fc			63
#define KEY_fcntl		64
#define KEY_fileno		65
#define KEY_flock		66
#define KEY_for			67
#define KEY_foreach		68
#define KEY_fork		69
#define KEY_format		70
#define KEY_formline		71
#define KEY_ge			72
#define KEY_getc		73
#define KEY_getgrent		74
#define KEY_getgrgid		75
#define KEY_getgrnam		76
#define KEY_gethostbyaddr	77
#define KEY_gethostbyname	78
#define KEY_gethostent		79
#define KEY_getlogin		80
#define KEY_getnetbyaddr	81
#define KEY_getnetbyname	82
#define KEY_getnetent		83
#define KEY_getpeername		84
#define KEY_getpgrp		85
#define KEY_getppid		86
#define KEY_getpriority		87
#define KEY_getprotobyname	88
#define KEY_getprotobynumber	89
#define KEY_getprotoent		90
#define KEY_getpwent		91
#define KEY_getpwnam		92
#define KEY_getpwuid		93
#define KEY_getservbyname	94
#define KEY_getservbyport	95
#define KEY_getservent		96
#define KEY_getsockname		97
#define KEY_getsockopt		98
#define KEY_given		99
#define KEY_glob		100
#define KEY_gmtime		101
#define KEY_goto		102
#define KEY_grep		103
#define KEY_gt			104
#define KEY_hex			105
#define KEY_if			106
#define KEY_index		107
#define KEY_int			108
#define KEY_ioctl		109
#define KEY_join		110
#define KEY_keys		111
#define KEY_kill		112
#define KEY_last		113
#define KEY_lc			114
#define KEY_lcfirst		115
#define KEY_le			116
#define KEY_length		117
#define KEY_link		118
#define KEY_listen		119
#define KEY_local		120
#define KEY_localtime		121
#define KEY_lock		122
#define KEY_log			123
#define KEY_lstat		124
#define KEY_lt			125
#define KEY_m			126
#define KEY_map			127
#define KEY_mkdir		128
#define KEY_msgctl		129
#define KEY_msgget		130
#define KEY_msgrcv		131
#define KEY_msgsnd		132
#define KEY_my			133
#define KEY_ne			134
#define KEY_next		135
#define KEY_no			136
#define KEY_not			137
#define KEY_oct			138
#define KEY_open		139
#define KEY_opendir		140
#define KEY_or			141
#define KEY_ord			142
#define KEY_our			143
#define KEY_pack		144
#define KEY_package		145
#define KEY_pipe		146
#define KEY_pop			147
#define KEY_pos			148
#define KEY_print		149
#define KEY_printf		150
#define KEY_prototype		151
#define KEY_push		152
#define KEY_q			153
#define KEY_qq			154
#define KEY_qr			155
#define KEY_quotemeta		156
#define KEY_qw			157
#define KEY_qx			158
#define KEY_rand		159
#define KEY_read		160
#define KEY_readdir		161
#define KEY_readline		162
#define KEY_readlink		163
#define KEY_readpipe		164
#define KEY_recv		165
#define KEY_redo		166
#define KEY_ref			167
#define KEY_rename		168
#define KEY_require		169
#define KEY_reset		170
#define KEY_return		171
#define KEY_reverse		172
#define KEY_rewinddir		173
#define KEY_rindex		174
#define KEY_rmdir		175
#define KEY_s			176
#define KEY_say			177
#define KEY_scalar		178
#define KEY_seek		179
#define KEY_seekdir		180
#define KEY_select		181
#define KEY_semctl		182
#define KEY_semget		183
#define KEY_semop		184
#define KEY_send		185
#define KEY_setgrent		186
#define KEY_sethostent		187
#define KEY_setnetent		188
#define KEY_setpgrp		189
#define KEY_setpriority		190
#define KEY_setprotoent		191
#define KEY_setpwent		192
#define KEY_setservent		193
#define KEY_setsockopt		194
#define KEY_shift		195
#define KEY_shmctl		196
#define KEY_shmget		197
#define KEY_shmread		198
#define KEY_shmwrite		199
#define KEY_shutdown		200
#define KEY_sin			201
#define KEY_sleep		202
#define KEY_socket		203
#define KEY_socketpair		204
#define KEY_sort		205
#define KEY_splice		206
#define KEY_split		207
#define KEY_sprintf		208
#define KEY_sqrt		209
#define KEY_srand		210
#define KEY_stat		211
#define KEY_state		212
#define KEY_study		213
#define KEY_sub			214
#define KEY_substr		215
#define KEY_symlink		216
#define KEY_syscall		217
#define KEY_sysopen		218
#define KEY_sysread		219
#define KEY_sysseek		220
#define KEY_system		221
#define KEY_syswrite		222
#define KEY_tell		223
#define KEY_telldir		224
#define KEY_tie			225
#define KEY_tied		226
#define KEY_time		227
#define KEY_times		228
#define KEY_tr			229
#define KEY_truncate		230
#define KEY_uc			231
#define KEY_ucfirst		232
#define KEY_umask		233
#define KEY_undef		234
#define KEY_unless		235
#define KEY_unlink		236
#define KEY_unpack		237
#define KEY_unshift		238
#define KEY_untie		239
#define KEY_until		240
#define KEY_use			241
#define KEY_utime		242
#define KEY_values		243
#define KEY_vec			244
#define KEY_wait		245
#define KEY_waitpid		246
#define KEY_wantarray		247
#define KEY_warn		248
#define KEY_when		249
#define KEY_while		250
#define KEY_write		251
#define KEY_x			252
#define KEY_xor			253
#define KEY_y			254

/* Generated from:
 * 7e3d76a333c5f9b77d47dd7d423450356b63853a1c2313d3e805042caaa4bc2c regen/keywords.pl
 * ex: set ro: */
