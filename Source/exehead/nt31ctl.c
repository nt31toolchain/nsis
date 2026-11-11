/*
 * nt31ctl.c
 *
 * This file is a part of NSIS.
 *
 * Licensed under the zlib/libpng license (the "License");
 * you may not use this file except in compliance with the License.
 *
 * Licence details can be found in the file COPYING.
 *
 * This software is provided 'as-is', without any express or implied
 * warranty.
 *
 * Replacements for the tree view, the list view and the progress bar,
 * for a system whose comctl32.dll does not have them (Windows NT 3.1).
 * They have only the messages that the pages of the installer and the
 * scripts use: the tree view is a flat list box with check marks in the
 * text, the list view a list box with one column.
 */

#include "../Platform.h"
#include <windowsx.h>
#include "state.h"
#include "util.h"
#include "ui.h"

#ifdef __NT31_SDK__

static WNDPROC listbox_procedure;

static LRESULT list(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
  return CallWindowProc(listbox_procedure, hwnd, message, wParam, lParam);
}

static TCHAR *copy_text(const TCHAR *text)
{
  int length = lstrlen(text);
  TCHAR *copy = (TCHAR *) GlobalAlloc(GPTR, (length + 1) * sizeof(TCHAR));
  if (copy) lstrcpy(copy, text);
  return copy;
}

static void replace_string(HWND hwnd, int index, const TCHAR *text, LRESULT data)
{
  LRESULT top = list(hwnd, LB_GETTOPINDEX, 0, 0);
  LRESULT selection = list(hwnd, LB_GETCURSEL, 0, 0);
  list(hwnd, WM_SETREDRAW, FALSE, 0);
  list(hwnd, LB_DELETESTRING, index, 0);
  list(hwnd, LB_INSERTSTRING, index, (LPARAM) text);
  list(hwnd, LB_SETITEMDATA, index, data);
  if (selection == index) list(hwnd, LB_SETCURSEL, index, 0);
  list(hwnd, LB_SETTOPINDEX, top, 0);
  list(hwnd, WM_SETREDRAW, TRUE, 0);
  InvalidateRect(hwnd, NULL, TRUE);
}

static void ensure_visible(HWND hwnd, int index)
{
  RECT client;
  int top = (int) list(hwnd, LB_GETTOPINDEX, 0, 0);
  int height = (int) list(hwnd, LB_GETITEMHEIGHT, 0, 0);
  int rows;

  GetClientRect(hwnd, &client);
  rows = height > 0 ? client.bottom / height : 1;
  if (rows < 1) rows = 1;
  if (index < top)
    list(hwnd, LB_SETTOPINDEX, index, 0);
  else if (index >= top + rows)
    list(hwnd, LB_SETTOPINDEX, index - rows + 1, 0);
}

static void notify(HWND hwnd, NMHDR *header, UINT code)
{
  header->hwndFrom = hwnd;
  header->idFrom = GetDlgCtrlID(hwnd);
  header->code = code;
  SendMessage(GetParent(hwnd), WM_NOTIFY, header->idFrom, (LPARAM) header);
}

static LRESULT create_list(HWND hwnd, CREATESTRUCT *create)
{
  create->style = (create->style & 0xFFFF0000) | WS_VSCROLL | LBS_NOINTEGRALHEIGHT;
  SetWindowLong(hwnd, GWL_STYLE, create->style);
  return list(hwnd, WM_NCCREATE, 0, (LPARAM) create);
}

/* The list view: the log of the installation. */

static LRESULT get_item_text(HWND hwnd, int index, LVITEM *item)
{
  LRESULT length = list(hwnd, LB_GETTEXTLEN, index, 0);
  TCHAR *text;

  if (length == LB_ERR || item->cchTextMax <= 0) return 0;
  text = (TCHAR *) GlobalAlloc(GPTR, (length + 1) * sizeof(TCHAR));
  if (!text) return 0;
  list(hwnd, LB_GETTEXT, index, (LPARAM) text);
  if (length > item->cchTextMax - 1) length = item->cchTextMax - 1;
  mini_memcpy(item->pszText, text, length * sizeof(TCHAR));
  item->pszText[length] = 0;
  GlobalFree(text);
  return length;
}

static LRESULT CALLBACK list_view_procedure(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
  LVITEM *item = (LVITEM *) lParam;
  LRESULT count;

  switch (message)
  {
  case WM_NCCREATE:
    return create_list(hwnd, (CREATESTRUCT *) lParam);
  case WM_RBUTTONUP:
    {
      POINT point = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
      ClientToScreen(hwnd, &point);
      SendMessage(GetParent(hwnd), WM_CONTEXTMENU, (WPARAM) hwnd, MAKELPARAM(point.x, point.y));
      return 0;
    }
  case LVM_GETITEMCOUNT:
    return list(hwnd, LB_GETCOUNT, 0, 0);
  case LVM_INSERTITEM:
    count = list(hwnd, LB_GETCOUNT, 0, 0);
    if (!(item->mask & LVIF_TEXT) || item->iSubItem) return -1;
    return list(hwnd, LB_INSERTSTRING, item->iItem < 0 || item->iItem >= count ? -1 : item->iItem,
                (LPARAM) item->pszText);
  case LVM_SETITEM:
    if (!(item->mask & LVIF_TEXT) || item->iSubItem) return FALSE;
    wParam = item->iItem;
    /* fall through */
  case LVM_SETITEMTEXT:
    if ((int) wParam < 0 || (LRESULT) wParam >= list(hwnd, LB_GETCOUNT, 0, 0)) return FALSE;
    replace_string(hwnd, (int) wParam, item->pszText, 0);
    return TRUE;
  case LVM_GETITEMTEXT:
    return get_item_text(hwnd, (int) wParam, item);
  case LVM_ENSUREVISIBLE:
    ensure_visible(hwnd, (int) wParam);
    return TRUE;
  case LVM_DELETEITEM:
    return list(hwnd, LB_DELETESTRING, wParam, 0) != LB_ERR;
  case LVM_DELETEALLITEMS:
    list(hwnd, LB_RESETCONTENT, 0, 0);
    return TRUE;
  case LVM_INSERTCOLUMN:
    return 0;
  case LVM_SETBKCOLOR:
  case LVM_SETTEXTBKCOLOR:
  case LVM_SETTEXTCOLOR:
    return TRUE;
  }
  if (message >= LVM_FIRST && message < LVM_FIRST + 0x100)
    return 0;
  return list(hwnd, message, wParam, lParam);
}

/* The tree view: the components. Each list box entry has a node as data;
   the nodes of a group follow it, in the order of the tree. */

struct node
{
  struct node *parent;
  int depth;
  UINT state;
  LPARAM lParam;
  TCHAR *text;
};

static struct node *node_at(HWND hwnd, LRESULT index)
{
  LRESULT data;
  if (index < 0) return NULL;
  data = list(hwnd, LB_GETITEMDATA, index, 0);
  return data == LB_ERR ? NULL : (struct node *) data;
}

static int index_of(HWND hwnd, const struct node *node)
{
  int count = (int) list(hwnd, LB_GETCOUNT, 0, 0), i;
  for (i = 0; i < count; i++)
    if (node_at(hwnd, i) == node) return i;
  return -1;
}

static int end_of_subtree(HWND hwnd, int index)
{
  int count = (int) list(hwnd, LB_GETCOUNT, 0, 0);
  int depth = node_at(hwnd, index)->depth;
  while (++index < count && node_at(hwnd, index)->depth > depth)
    ;
  return index;
}

static TCHAR *row_text(const struct node *node)
{
  static const TCHAR *const marks[] = {
    _T("[ ] "), _T("[ ] "), _T("[X] "), _T("[-] "), _T("< > "), _T("<X> ")
  };
  UINT image = (node->state & TVIS_STATEIMAGEMASK) >> 12;
  int indent = node->depth * 4, i;
  TCHAR *row = (TCHAR *) GlobalAlloc(GPTR, (indent + 5 + lstrlen(node->text)) * sizeof(TCHAR));

  if (!row) return NULL;
  for (i = 0; i < indent; i++) row[i] = _T(' ');
  lstrcpy(row + indent, image < COUNTOF(marks) ? marks[image] : marks[0]);
  lstrcat(row, node->text);
  return row;
}

static void show_row(HWND hwnd, int index, struct node *node, BOOL insert)
{
  TCHAR *row = row_text(node);
  if (!row) return;
  if (insert)
  {
    list(hwnd, LB_INSERTSTRING, index, (LPARAM) row);
    list(hwnd, LB_SETITEMDATA, index, (LPARAM) node);
  }
  else
    replace_string(hwnd, index, row, (LRESULT) node);
  GlobalFree(row);
}

static void set_item(struct node *node, const TVITEM *item)
{
  if (item->mask & TVIF_STATE)
    node->state = (node->state & ~item->stateMask) | (item->state & item->stateMask);
  if (item->mask & TVIF_PARAM)
    node->lParam = item->lParam;
  if ((item->mask & TVIF_TEXT) && item->pszText != LPSTR_TEXTCALLBACK)
  {
    TCHAR *text = copy_text(item->pszText);
    if (text)
    {
      if (node->text) GlobalFree(node->text);
      node->text = text;
    }
  }
}

static HTREEITEM insert_item(HWND hwnd, const TVINSERTSTRUCT *insert)
{
  struct node *parent = (struct node *) insert->hParent;
  struct node *node;
  int index;

  if (parent == (struct node *) TVI_ROOT) parent = NULL;
  if (insert->hInsertAfter == TVI_FIRST)
    index = parent ? index_of(hwnd, parent) + 1 : 0;
  else if (insert->hInsertAfter != TVI_LAST && insert->hInsertAfter != TVI_SORT && insert->hInsertAfter)
    index = end_of_subtree(hwnd, index_of(hwnd, (struct node *) insert->hInsertAfter));
  else if (parent)
    index = end_of_subtree(hwnd, index_of(hwnd, parent));
  else
    index = (int) list(hwnd, LB_GETCOUNT, 0, 0);

  node = (struct node *) GlobalAlloc(GPTR, sizeof(*node));
  if (!node) return NULL;
  node->parent = parent;
  node->depth = parent ? parent->depth + 1 : 0;
  node->text = copy_text(_T(""));
  set_item(node, &insert->item);
  show_row(hwnd, index, node, TRUE);
  return (HTREEITEM) node;
}

static void get_item(const struct node *node, TVITEM *item)
{
  if (item->mask & TVIF_STATE)
    item->state = node->state & item->stateMask;
  if (item->mask & TVIF_PARAM)
    item->lParam = node->lParam;
  if ((item->mask & TVIF_TEXT) && item->cchTextMax > 0)
  {
    int length = lstrlen(node->text);
    if (length > item->cchTextMax - 1) length = item->cchTextMax - 1;
    mini_memcpy(item->pszText, node->text, length * sizeof(TCHAR));
    item->pszText[length] = 0;
  }
}

static void delete_rows(HWND hwnd, int first, int end)
{
  while (end-- > first)
  {
    struct node *node = node_at(hwnd, first);
    list(hwnd, LB_DELETESTRING, first, 0);
    if (node->text) GlobalFree(node->text);
    GlobalFree(node);
  }
}

static HTREEITEM next_item(HWND hwnd, UINT code, struct node *node)
{
  int count = (int) list(hwnd, LB_GETCOUNT, 0, 0);
  int index = node ? index_of(hwnd, node) : -1;
  struct node *next;

  switch (code)
  {
  case TVGN_ROOT:
    return (HTREEITEM) node_at(hwnd, 0);
  case TVGN_CARET:
    return (HTREEITEM) node_at(hwnd, list(hwnd, LB_GETCURSEL, 0, 0));
  case TVGN_PARENT:
    return node ? (HTREEITEM) node->parent : NULL;
  case TVGN_CHILD:
    next = index >= 0 && index + 1 < count ? node_at(hwnd, index + 1) : NULL;
    return next && next->parent == node ? (HTREEITEM) next : NULL;
  case TVGN_NEXT:
    if (index < 0) return NULL;
    index = end_of_subtree(hwnd, index);
    next = index < count ? node_at(hwnd, index) : NULL;
    return next && next->parent == node->parent ? (HTREEITEM) next : NULL;
  }
  return NULL;
}

static HTREEITEM hit_test(HWND hwnd, TVHITTESTINFO *test)
{
  RECT client;
  int height = (int) list(hwnd, LB_GETITEMHEIGHT, 0, 0);
  int index;

  GetClientRect(hwnd, &client);
  test->flags = TVHT_NOWHERE;
  test->hItem = NULL;
  if (height <= 0 || !PtInRect(&client, test->pt)) return NULL;
  index = (int) list(hwnd, LB_GETTOPINDEX, 0, 0) + test->pt.y / height;
  if (index >= (int) list(hwnd, LB_GETCOUNT, 0, 0)) return NULL;
  test->flags = TVHT_ONITEMSTATEICON | TVHT_ONITEMLABEL;
  test->hItem = (HTREEITEM) node_at(hwnd, index);
  return test->hItem;
}

static void notify_selection(HWND hwnd, LRESULT before)
{
  LRESULT after = list(hwnd, LB_GETCURSEL, 0, 0);
  struct node *node = node_at(hwnd, after);
  NMTREEVIEW change = {0};

  if (after == before || !node) return;
  change.itemNew.mask = TVIF_HANDLE | TVIF_PARAM | TVIF_STATE;
  change.itemNew.hItem = (HTREEITEM) node;
  change.itemNew.state = node->state;
  change.itemNew.lParam = node->lParam;
  notify(hwnd, &change.hdr, TVN_SELCHANGED);
}

static LRESULT CALLBACK tree_view_procedure(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
  struct node *node = (struct node *) lParam;
  LRESULT selection, result;
  int index;

  switch (message)
  {
  case WM_NCCREATE:
    return create_list(hwnd, (CREATESTRUCT *) lParam);
  case WM_DESTROY:
    delete_rows(hwnd, 0, (int) list(hwnd, LB_GETCOUNT, 0, 0));
    break;
  case WM_LBUTTONDOWN:
  case WM_LBUTTONDBLCLK:
  case WM_KEYDOWN:
    selection = list(hwnd, LB_GETCURSEL, 0, 0);
    result = list(hwnd, message, wParam, lParam);
    notify_selection(hwnd, selection);
    if (message != WM_KEYDOWN)
    {
      NMHDR click;
      notify(hwnd, &click, NM_CLICK);
    }
    return result;
  case TVM_INSERTITEM:
    return (LRESULT) insert_item(hwnd, (const TVINSERTSTRUCT *) lParam);
  case TVM_SETITEM:
    {
      const TVITEM *item = (const TVITEM *) lParam;
      node = (struct node *) item->hItem;
      index = index_of(hwnd, node);
      if (index < 0) return FALSE;
      set_item(node, item);
      show_row(hwnd, index, node, FALSE);
      return TRUE;
    }
  case TVM_GETITEM:
    {
      TVITEM *item = (TVITEM *) lParam;
      node = (struct node *) item->hItem;
      if (index_of(hwnd, node) < 0) return FALSE;
      get_item(node, item);
      return TRUE;
    }
  case TVM_DELETEITEM:
    if (!node || node == (struct node *) TVI_ROOT)
    {
      delete_rows(hwnd, 0, (int) list(hwnd, LB_GETCOUNT, 0, 0));
      return TRUE;
    }
    index = index_of(hwnd, node);
    if (index < 0) return FALSE;
    delete_rows(hwnd, index, end_of_subtree(hwnd, index));
    return TRUE;
  case TVM_GETNEXTITEM:
    return (LRESULT) next_item(hwnd, (UINT) wParam, node);
  case TVM_SELECTITEM:
    if (wParam != TVGN_CARET) return FALSE;
    selection = list(hwnd, LB_GETCURSEL, 0, 0);
    list(hwnd, LB_SETCURSEL, node ? index_of(hwnd, node) : -1, 0);
    notify_selection(hwnd, selection);
    return TRUE;
  case TVM_ENSUREVISIBLE:
    index = index_of(hwnd, node);
    if (index >= 0) ensure_visible(hwnd, index);
    return FALSE;
  case TVM_HITTEST:
    return (LRESULT) hit_test(hwnd, (TVHITTESTINFO *) lParam);
  case TVM_GETCOUNT:
    return list(hwnd, LB_GETCOUNT, 0, 0);
  case TVM_GETITEMHEIGHT:
    return list(hwnd, LB_GETITEMHEIGHT, 0, 0);
  case TVM_EXPAND:
    return TRUE;
  }
  if (message >= TV_FIRST && message < TV_FIRST + 0x100)
    return 0;
  return list(hwnd, message, wParam, lParam);
}

/* The progress bar. */

struct progress
{
  int low, high, position, step;
  COLORREF bar, background;
};

static LRESULT CALLBACK progress_procedure(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
  struct progress *bar = (struct progress *) GetWindowLongPtr(hwnd, 0);
  int old;

  switch (message)
  {
  case WM_NCCREATE:
    bar = (struct progress *) GlobalAlloc(GPTR, sizeof(*bar));
    if (!bar) return FALSE;
    bar->high = 100;
    bar->step = 10;
    bar->bar = bar->background = CLR_DEFAULT;
    SetWindowLongPtr(hwnd, 0, (LONG_PTR) bar);
    break;
  case WM_NCDESTROY:
    if (bar) GlobalFree(bar);
    SetWindowLongPtr(hwnd, 0, 0);
    break;
  case WM_ERASEBKGND:
    return TRUE;
  case WM_PAINT:
    {
      PAINTSTRUCT paint;
      RECT client, done;
      HBRUSH brush;
      int range = bar->high - bar->low;

      BeginPaint(hwnd, &paint);
      GetClientRect(hwnd, &client);
      done = client;
      done.right = range > 0 ? client.left + MulDiv(client.right - client.left, bar->position - bar->low, range)
                             : client.left;
      brush = CreateSolidBrush(bar->bar == CLR_DEFAULT ? GetSysColor(COLOR_HIGHLIGHT) : bar->bar);
      FillRect(paint.hdc, &done, brush);
      DeleteObject(brush);
      client.left = done.right;
      brush = CreateSolidBrush(bar->background == CLR_DEFAULT ? GetSysColor(COLOR_BTNFACE) : bar->background);
      FillRect(paint.hdc, &client, brush);
      DeleteObject(brush);
      EndPaint(hwnd, &paint);
      return 0;
    }
  case PBM_SETRANGE:
  case PBM_SETRANGE32:
    old = MAKELONG(bar->low, bar->high);
    bar->low = message == PBM_SETRANGE ? LOWORD(lParam) : (int) wParam;
    bar->high = message == PBM_SETRANGE ? HIWORD(lParam) : (int) lParam;
    if (bar->high < bar->low) bar->high = bar->low;
    wParam = bar->position;
    /* fall through */
  case PBM_SETPOS:
  case PBM_DELTAPOS:
  case PBM_STEPIT:
    if (message == PBM_SETPOS || message == PBM_DELTAPOS || message == PBM_STEPIT)
      old = bar->position;
    if (message == PBM_DELTAPOS) wParam = bar->position + (int) wParam;
    if (message == PBM_STEPIT)
    {
      wParam = bar->position + bar->step;
      if ((int) wParam > bar->high) wParam = bar->low + ((int) wParam - bar->high);
    }
    bar->position = (int) wParam;
    if (bar->position < bar->low) bar->position = bar->low;
    if (bar->position > bar->high) bar->position = bar->high;
    InvalidateRect(hwnd, NULL, FALSE);
    return old;
  case PBM_SETSTEP:
    old = bar->step;
    bar->step = (int) wParam;
    return old;
  case PBM_GETPOS:
    return bar->position;
  case PBM_SETBARCOLOR:
  case PBM_SETBKCOLOR:
    if (message == PBM_SETBARCOLOR)
    {
      old = bar->bar;
      bar->bar = (COLORREF) lParam;
    }
    else
    {
      old = bar->background;
      bar->background = (COLORREF) lParam;
    }
    InvalidateRect(hwnd, NULL, FALSE);
    return old;
  }
  if (message >= WM_USER && message < WM_USER + 0x100)
    return 0;
  return DefWindowProc(hwnd, message, wParam, lParam);
}

static void register_list(const TCHAR *name, WNDPROC procedure)
{
  WNDCLASS wc;
  if (GetClassInfo(NULL, name, &wc) || !GetClassInfo(NULL, _T("LISTBOX"), &wc))
    return;
  listbox_procedure = wc.lpfnWndProc;
  wc.lpfnWndProc = procedure;
  wc.hInstance = g_hInstance;
  wc.style |= CS_GLOBALCLASS;
  wc.lpszClassName = name;
  RegisterClass(&wc);
}

void NSISCALL RegisterNT31Controls(void)
{
  WNDCLASS wc = {0};

  register_list(WC_TREEVIEW, tree_view_procedure);
  register_list(WC_LISTVIEW, list_view_procedure);
  if (!GetClassInfo(NULL, PROGRESS_CLASS, &wc))
  {
    wc.style = CS_GLOBALCLASS | CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = progress_procedure;
    wc.cbWndExtra = sizeof(struct progress *);
    wc.hInstance = g_hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = PROGRESS_CLASS;
    RegisterClass(&wc);
  }
}

#endif
