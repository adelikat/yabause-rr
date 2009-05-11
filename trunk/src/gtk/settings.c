/*  Copyright 2006 Guillaume Duhamel
    Copyright 2006 Fabien Coulon

    This file is part of Yabause.

    Yabause is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    Yabause is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Yabause; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "settings.h"

#include "yuifileentry.h"
#include "yuirange.h"
#include "yuipage.h"
#include "yuiinputentry.h"
#include "yuiresolution.h"

YuiRangeItem sh2interpreters[] = {
  { "0", "Fast Interpreter" },
  { "1", "Debug Interpreter" },
#ifdef TEST_PSP_SH2
  { "1486", "PSP Interpreter (for debugging)" },
#endif
  { 0, 0 }
};

YuiRangeItem m68kinterpreters[] = {
  { "1", "Standard Interpreter" },
  { "2", "Q68 Interpreter" },
  { "0", "Disabled" },
  { 0, 0 }
};

YuiRangeItem regions[] = {
	{ "Auto" , "Auto-detect" },
	{ "J" , "Japan" },
	{ "T", "Asia" },
	{ "U", "North America" },
	{ "B", "Central/South America" },
	{ "K", "Korea" },
	{ "A", "Asia" },
	{ "E", "Europe + others" },
	{ "L", "Central/South America" },
	{ 0, 0 }
};

YuiRangeItem carttypes[] = {
	{ "0", "None" },
	{ "1", "Pro Action Replay" },
	{ "2", "4 Mbit Backup Ram" },
	{ "3", "8 Mbit Backup Ram" },
	{ "4", "16 Mbit Backup Ram" },
	{ "5", "32 Mbit Backup Ram" },
	{ "6", "8 Mbit Dram" },
	{ "7", "32 Mbit Dram" },
	{ "8", "Netlink" },
	{ "9", "16 Mbit ROM" },
	{ 0, 0 }
};

YuiRangeItem cdcores[] = {
	{ "0" , "Dummy CD" },
	{ "1" , "ISO or CUE file" },
#ifndef UNKNOWN_ARCH
	{ "2" , "Cdrom device" },
#endif
	{ 0, 0 }
};

YuiRangeItem vidcores[] = {
	{ "0", "Dummy Video Interface" },
#ifdef HAVE_LIBGTKGLEXT
	{ "1", "OpenGL Video Interface" },
#endif
	{ "2", "Software Video Interface" },
	{ 0, 0 }
};

YuiRangeItem sndcores[] = {
	{ "0", "Dummy Sound Interface" },
#ifdef HAVE_LIBSDL
	{ "1", "SDL Sound Interface" },
#endif
#ifdef HAVE_LIBAL
	{ "2", "OpenAL Sound Interface" },
#endif
	{ 0, 0 }
};

const gchar * keys1[] = { "Up", "Right", "Down", "Left", "R", "L", 0 };
const gchar * keys2[] = { "A", "B", "C", "X", "Y", "Z", "Start", 0 };

YuiRangeItem vidformats[] = {
	{ "0", "NTSC" },
	{ "1", "PAL" },
	{ 0, 0 }
};

YuiRangeItem percores[] = {
	{ "2", "Gtk Input Interface" },
#ifdef HAVE_LIBSDL
	{ "3", "SDL Joystick Interface" },
#endif
#ifdef ARCH_IS_LINUX
	{ "4", "Linux Joystick Interface" },
#endif
	{ 0, 0 }
};

YuiRangeItem mousepercores[] = {
	{ "2", "Gtk Input Interface" },
	{ 0, 0 }
};

void hide_show_cart_path(YuiRange * instance, gpointer data) {
	gint i = yui_range_get_active(instance);

	if (i == 8) {
		gtk_widget_hide(data);
	} else {
		gtk_widget_show(data);
	}
}

void hide_show_netlink(YuiRange * instance, gpointer data) {
	gint i = yui_range_get_active(instance);

	if (i != 8) {
		gtk_widget_hide(data);
	} else {
		gtk_widget_show(data);
	}
}

void percore_changed(GtkWidget * widget, gpointer data) {
	const char * core_s = percores[gtk_combo_box_get_active(GTK_COMBO_BOX(widget))].value;
	GList * entrylist = data;
	int core;
	sscanf(core_s, "%d", &core);

	PerDeInit();
	PerInit(core);

	while(entrylist) {
		yui_input_entry_update(entrylist->data);
		entrylist = g_list_next(entrylist);
	}
}

static void pertype_display_pad(GtkWidget * box)
{
   GtkWidget * table4, * table5;
   GtkWidget * box_percore = gtk_vbox_new(FALSE, 10);
   GtkWidget * select_percore = yui_range_new(keyfile, "General", "PerCore", percores);
   GList * entrylist = NULL;

   gtk_container_set_border_width(GTK_CONTAINER(select_percore), 0);

   gtk_container_set_border_width(GTK_CONTAINER(box_percore), 10);

   gtk_box_pack_start(GTK_BOX (box_percore), select_percore, FALSE, FALSE, 0);

   table4 = yui_input_entry_new(keyfile, "Pad", keys1);
   entrylist = g_list_append(entrylist, table4);

   gtk_box_pack_start (GTK_BOX (box_percore), table4, TRUE, TRUE, 0);

   gtk_box_pack_start (GTK_BOX (box), box_percore, TRUE, TRUE, 0);

   gtk_box_pack_start (GTK_BOX (box), gtk_vseparator_new(), TRUE, TRUE, 0);

   table5 = yui_input_entry_new(keyfile, "Pad", keys2);
   entrylist = g_list_append(entrylist, table5);

   gtk_container_set_border_width(GTK_CONTAINER(table5), 10);
  
   gtk_box_pack_start (GTK_BOX (box), table5, TRUE, TRUE, 0);

   g_signal_connect(GTK_COMBO_BOX(YUI_RANGE(select_percore)->combo), "changed", G_CALLBACK(percore_changed), entrylist);

   gtk_widget_show_all(box);
}

static void mouse_speed_change(GtkWidget * range, gpointer data)
{
   g_key_file_set_double(keyfile, "General", "MouseSpeed", gtk_range_get_value(GTK_RANGE(range)));
}

static void pertype_display_mouse(GtkWidget * box)
{
   GtkWidget * scale;
   GtkWidget * table5;
   GtkWidget * box_percore = gtk_vbox_new(FALSE, 10);
   GtkWidget * select_percore = yui_range_new(keyfile, "General", "MousePerCore", mousepercores);
   GList * entrylist = NULL;

   gtk_container_set_border_width(GTK_CONTAINER(select_percore), 0);
   gtk_container_set_border_width(GTK_CONTAINER(box_percore), 10);
   gtk_box_pack_start(GTK_BOX (box_percore), select_percore, FALSE, FALSE, 0);

   scale = gtk_hscale_new_with_range(0, 10, 0.1);
   gtk_range_set_value(GTK_RANGE(scale), g_key_file_get_double(keyfile, "General", "MouseSpeed", NULL));
   g_signal_connect(scale, "value-changed", G_CALLBACK(mouse_speed_change), NULL);
   gtk_box_pack_start(GTK_BOX (box_percore), scale, FALSE, FALSE, 0);

   gtk_box_pack_start (GTK_BOX (box), box_percore, TRUE, TRUE, 0);

   gtk_box_pack_start (GTK_BOX (box), gtk_vseparator_new(), TRUE, TRUE, 0);

   table5 = yui_input_entry_new(keyfile, "Mouse", PerMouseNames);
   entrylist = g_list_append(entrylist, table5);
   gtk_container_set_border_width(GTK_CONTAINER(table5), 10);
   gtk_box_pack_start (GTK_BOX (box), table5, TRUE, TRUE, 0);

   g_signal_connect(GTK_COMBO_BOX(YUI_RANGE(select_percore)->combo), "changed", G_CALLBACK(percore_changed), entrylist);
   gtk_widget_show_all(box);
}

void pertype_changed(GtkWidget * widget, gpointer data) {
	GtkTreePath * path;
	gchar * strpath;
	int i;
	GtkWidget * box = data;
	GList * children;
	GtkWidget * child;

	children = gtk_container_get_children(GTK_CONTAINER(box));
	for(i = 1;i < 4;i++) {
		child = g_list_nth_data(children, i);
		if (child != NULL) gtk_widget_destroy(child);
	}

	gtk_tree_view_get_cursor(GTK_TREE_VIEW(widget), &path, NULL);

	if (path) {
		int i;

		strpath = gtk_tree_path_to_string(path);
		sscanf(strpath, "%d", &i);

		switch(i) {
			case 0:
				g_key_file_set_integer(keyfile, "General", "PerType", PERPAD);
				pertype_display_pad(box);
				break;
			case 1:
				g_key_file_set_integer(keyfile, "General", "PerType", PERMOUSE);
				pertype_display_mouse(box);
				break;
		}

		g_free(strpath);
		gtk_tree_path_free(path);
	}
}

GtkWidget* create_dialog1(void) {
  GtkWidget *dialog1;
  GtkWidget *notebook1;
  GtkWidget *vbox17;
  GtkWidget *hbox22;
  GtkWidget *button11;
  GtkWidget *button12;
  GtkWidget * general, * video_sound, * cart_memory, *advanced, * sound;
  GtkWidget * box;
  u8 perid;

  dialog1 = gtk_dialog_new ();
  gtk_window_set_title (GTK_WINDOW (dialog1), "Yabause configuration");
  gtk_window_set_icon_name (GTK_WINDOW (dialog1), "gtk-preferences");
  gtk_window_set_type_hint (GTK_WINDOW (dialog1), GDK_WINDOW_TYPE_HINT_DIALOG);
  gtk_window_set_resizable(GTK_WINDOW(dialog1), FALSE);

  notebook1 = gtk_notebook_new ();
  gtk_widget_show(notebook1);
  
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog1)->vbox), notebook1, TRUE, TRUE, 0);

  /*
   * General configuration
   */
  general = yui_page_new(keyfile);

  box = yui_page_add(YUI_PAGE(general), _("Bios"));
  gtk_container_add(GTK_CONTAINER(box), yui_file_entry_new(keyfile, "General", "BiosPath", YUI_FILE_ENTRY_BROWSE, NULL));

  box = yui_page_add(YUI_PAGE(general), _("Cdrom"));
  gtk_container_add(GTK_CONTAINER(box), yui_range_new(keyfile, "General", "CDROMCore", cdcores));
  gtk_container_add(GTK_CONTAINER(box), yui_file_entry_new(keyfile, "General", "CDROMDrive", YUI_FILE_ENTRY_BROWSE, NULL));

  box = yui_page_add(YUI_PAGE(general), _("Save States"));
  gtk_container_add(GTK_CONTAINER(box), yui_file_entry_new(keyfile, "General", "StatePath", YUI_FILE_ENTRY_BROWSE | YUI_FILE_ENTRY_DIRECTORY, NULL));
  
  gtk_notebook_append_page(GTK_NOTEBOOK(notebook1), general, gtk_label_new (_("General")));
  gtk_widget_show_all(general);

  /*
   * Video configuration
   */
  video_sound = yui_page_new(keyfile);

  box = yui_page_add(YUI_PAGE(video_sound), _("Video Core"));
  gtk_container_add(GTK_CONTAINER(box), yui_range_new(keyfile, "General", "VideoCore", vidcores));

  box = yui_page_add(YUI_PAGE(video_sound), _("Resolution"));
  gtk_container_add(GTK_CONTAINER(box), yui_resolution_new(keyfile, "General"));

  box = yui_page_add(YUI_PAGE(video_sound), _("Video Format"));
  gtk_container_add(GTK_CONTAINER(box), yui_range_new(keyfile, "General", "VideoFormat", vidformats));
  
  gtk_notebook_append_page(GTK_NOTEBOOK(notebook1), video_sound, gtk_label_new (_("Video")));
  gtk_widget_show_all(video_sound);

  /*
   * Sound configuration
   */
  sound = yui_page_new(keyfile);

  box = yui_page_add(YUI_PAGE(sound), _("Sound Core"));
  gtk_container_add(GTK_CONTAINER(box), yui_range_new(keyfile, "General", "SoundCore", sndcores));
  
  gtk_notebook_append_page(GTK_NOTEBOOK(notebook1), sound, gtk_label_new (_("Sound")));
  gtk_widget_show_all(sound);

  /*
   * Cart/Memory configuration
   */
  cart_memory = yui_page_new(keyfile);
  
  box = yui_page_add(YUI_PAGE(cart_memory), _("Cartridge"));
  {
     GtkWidget * w1, * w2, * w3;

     w1 = yui_range_new(keyfile, "General", "CartType", carttypes);
     gtk_container_add(GTK_CONTAINER(box), w1);

     w2 = yui_file_entry_new(keyfile, "General", "CartPath", YUI_FILE_ENTRY_BROWSE, NULL);
     gtk_container_add(GTK_CONTAINER(box), w2);

     w3 = gtk_hbox_new(FALSE, 0);
     gtk_box_pack_start(GTK_BOX(w3), yui_file_entry_new(keyfile, "General", "NetlinkHost", 0, "Host"), TRUE, TRUE, 0);
     gtk_box_pack_start(GTK_BOX(w3), yui_file_entry_new(keyfile, "General", "NetlinkPort", 0, "Port"), TRUE, TRUE, 0);
     gtk_container_add(GTK_CONTAINER(box), w3);

     g_signal_connect(w1, "changed", G_CALLBACK(hide_show_cart_path), w2);
     g_signal_connect(w1, "changed", G_CALLBACK(hide_show_netlink), w3);
  
     box = yui_page_add(YUI_PAGE(cart_memory), _("Memory"));
     gtk_container_add(GTK_CONTAINER(box), yui_file_entry_new(keyfile, "General", "BackupRamPath", YUI_FILE_ENTRY_BROWSE, NULL));

     box = yui_page_add(YUI_PAGE(cart_memory), _("Mpeg ROM"));
     gtk_container_add(GTK_CONTAINER(box), yui_file_entry_new(keyfile, "General", "MpegRomPath", YUI_FILE_ENTRY_BROWSE, NULL));
  
     gtk_notebook_append_page(GTK_NOTEBOOK(notebook1), cart_memory, gtk_label_new (_("Cart/Memory")));
     gtk_widget_show_all(cart_memory);

     if (yui_range_get_active(YUI_RANGE(w1)) == 8) gtk_widget_hide(w2); 
     else gtk_widget_hide(w3);
  }

  /*
   * Input Configuration
   */
  vbox17 = gtk_vbox_new (FALSE, 0);
  
  hbox22 = gtk_hbox_new (FALSE, 0);

  {
    GtkWidget * controllerscroll;
    GtkTreeStore * controllerlist;
    GtkWidget * controllerlistview;
    GtkCellRenderer * controllerrenderer;
    GtkTreeViewColumn * controllercolumn;
    GtkTreeIter iter;
    GtkTreePath * path;

    controllerscroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(controllerscroll), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

    controllerlist = gtk_tree_store_new(1, G_TYPE_STRING);
    controllerlistview = gtk_tree_view_new_with_model(GTK_TREE_MODEL(controllerlist));
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(controllerlistview), FALSE);

    controllerrenderer = gtk_cell_renderer_text_new();
    controllercolumn = gtk_tree_view_column_new_with_attributes("Controller", controllerrenderer, "text", 0, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW (controllerlistview), controllercolumn);

    gtk_tree_store_append(controllerlist, &iter, NULL);
    gtk_tree_store_set(controllerlist, &iter, 0, "Pad", -1);

    gtk_tree_store_append(controllerlist, &iter, NULL);
    gtk_tree_store_set(controllerlist, &iter, 0, "Mouse", -1);

    gtk_container_add(GTK_CONTAINER(controllerscroll), controllerlistview);
    gtk_box_pack_start (GTK_BOX (hbox22), controllerscroll, TRUE, TRUE, 0);

    gtk_tree_view_expand_all(GTK_TREE_VIEW(controllerlistview));

    g_signal_connect(controllerlistview, "cursor-changed", G_CALLBACK(pertype_changed), hbox22);
    perid = g_key_file_get_integer(keyfile, "General", "PerType", NULL);
    switch(perid)
    {
       case PERMOUSE:
          path = gtk_tree_path_new_from_string("1");
          break;
       case PERPAD:
       default:
          path = gtk_tree_path_new_from_string("0");
          break;
    }
          
    gtk_tree_view_set_cursor(GTK_TREE_VIEW(controllerlistview), path, NULL, FALSE);
    gtk_tree_path_free(path);
  }
  
  gtk_box_pack_start (GTK_BOX (vbox17), hbox22, TRUE, TRUE, 0);

  //pertype_display_pad(hbox22);

  gtk_notebook_append_page(GTK_NOTEBOOK(notebook1), vbox17, gtk_label_new (_("Input")));
  gtk_widget_show_all(vbox17);

  /*
   * Advanced configuration
   */

  advanced = yui_page_new(keyfile);
  
  box = yui_page_add(YUI_PAGE(advanced), _("Region"));
  gtk_container_add(GTK_CONTAINER(box), yui_range_new(keyfile, "General", "Region", regions));

  box = yui_page_add(YUI_PAGE(advanced), _("SH2 Interpreter"));
  gtk_container_add(GTK_CONTAINER(box), yui_range_new(keyfile, "General", "SH2Int", sh2interpreters));

  box = yui_page_add(YUI_PAGE(advanced), _("M68k Interpreter"));
  gtk_container_add(GTK_CONTAINER(box), yui_range_new(keyfile, "General", "M68kInt", m68kinterpreters));

#ifdef HAVE_LIBMINI18N
  box = yui_page_add(YUI_PAGE(advanced), _("Translation"));
  gtk_container_add(GTK_CONTAINER(box), yui_file_entry_new(keyfile, "General", "TranslationPath", YUI_FILE_ENTRY_BROWSE, NULL));
#endif
  
  gtk_notebook_append_page(GTK_NOTEBOOK(notebook1), advanced, gtk_label_new (_("Advanced")));
  gtk_widget_show_all(advanced);

  /*
   * Dialog buttons
   */
  button11 = gtk_button_new_from_stock ("gtk-cancel");
  
  gtk_dialog_add_action_widget (GTK_DIALOG (dialog1), button11, GTK_RESPONSE_CANCEL);
  GTK_WIDGET_SET_FLAGS (button11, GTK_CAN_DEFAULT);
  gtk_widget_show(button11);

  button12 = gtk_button_new_from_stock ("gtk-ok");
  
  gtk_dialog_add_action_widget (GTK_DIALOG (dialog1), button12, GTK_RESPONSE_OK);
  GTK_WIDGET_SET_FLAGS (button12, GTK_CAN_DEFAULT);
  gtk_widget_show(button12);

  gtk_widget_show(dialog1);

  return dialog1;
}
