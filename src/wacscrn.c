/*****************************************************************************
** wacscrn.c
**
** Copyright (C) 2002 - John E. Joganic
**
** This program is free software; you can redistribute it and/or
** modify it under the terms of the GNU General Public License
** as published by the Free Software Foundation; either version 2
** of the License, or (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU Lesser General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
**
****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <ncurses.h>

/****************************************************************************/

void wacscrn_init(void)
	{ initscr(); }
void wacscrn_term(void)
	{ endwin(); }
void wacscrn_output(int y, int x, const char* pszText)
	{ mvaddstr(y,x,pszText); }
void wacscrn_standout(void)
	{ attron(A_STANDOUT); }
void wacscrn_normal(void)
	{ attrset(A_NORMAL); }
void wacscrn_refresh(void)
	{ refresh(); }
