#!/usr/bin/env python
# -*- coding: utf-8 -*-
''' bto_advanced_USBIR_cmdのシンプルなラッパー
    作成者 disklessfun@gmail.com
'''

import gtk
import sys
import os
import commands

class ir_remocon_advance(gtk.Window):
    def __init__(self, parent=None):
        # Create the toplevel window
        gtk.Window.__init__(self)
        self.set_default_size(600,550)
        self.set_border_width(8)

        try:
            self.set_screen(parent.get_screen())
        except AttributeError:
            self.connect('destroy', lambda *w: gtk.main_quit())

        self.set_title('赤外線リモコンアドバンス・生データ送受ツール')


        # main_vbox
        main_vbox = gtk.VBox(False, 8)
        # add main_vbox
        self.add(main_vbox)

        # frame_upper
        frame_upper = gtk.Frame()
        frame_upper.set_size_request(600, 40)
        # hbox
        hbox = gtk.HButtonBox()
        hbox.set_layout(gtk.BUTTONBOX_SPREAD)

        # add hbox
        frame_upper.add(hbox)

        # button_1
        button_1 = gtk.Button()
        button_1.set_label('受信開始')
        button_1.connect('clicked', self.recUSBIRData_Start)
        # add button_1
        hbox.add(button_1)
        # button_2
        button_2 = gtk.Button()
        button_2.set_label('受信停止')
        button_2.connect('clicked', self.recUSBIRData_Stop)
        # add button_2
        hbox.add(button_2)
        # button_3
        button_3 = gtk.Button()
        button_3.set_label('データ取得')
        button_3.connect('clicked', self.readUSBIRData)
        # add button_3
        hbox.add(button_3)
        # button_4
        button_4 = gtk.Button()
        button_4.set_label('送信')
        button_4.connect('clicked', self.writeUSBIRData)
        # add button_4
        hbox.add(button_4)

        # pack_start frame_upper
        main_vbox.pack_start(frame_upper, padding=0)

        # seif.frame_lower
        self.frame_lower = gtk.Frame()
        # pack_start self.frame_lower
        main_vbox.pack_start(self.frame_lower, padding=0)

        # self.textview        
        self.textview = gtk.TextView()
        self.textview.set_size_request(-1, 600)
        self.textview.set_wrap_mode(gtk.WRAP_CHAR)
        self.textview.set_editable(True)
        # self.textbuffer
        self.textbuffer = self.textview.get_buffer()
        self.textbuffer.set_text('')

        # scrolledwindow
        scrolledwindow = gtk.ScrolledWindow()
        scrolledwindow.set_policy(gtk.POLICY_AUTOMATIC, gtk.POLICY_AUTOMATIC)
        scrolledwindow.add(self.textview)

        # add scrolledwindow
        self.frame_lower.add(scrolledwindow)

        self.show_all()


    def recUSBIRData_Start(self, widget, data=None):
        status=commands.getoutput("bto_advanced_USBIR_cmd -r")
        self.textbuffer.set_text(status)
        return False

    def recUSBIRData_Stop(self, widget, data=None):
        status=commands.getoutput("bto_advanced_USBIR_cmd -s")
        self.textbuffer.set_text(status)
        return False

    def readUSBIRData(self, widget, data=None):
        status=commands.getoutput("bto_advanced_USBIR_cmd -g")
        self.textbuffer.set_text(status)
        return False

    def writeUSBIRData(self, widget, data=None):
        startier, endtier = self.textbuffer.get_bounds()
        text =self.textbuffer.get_text(startier, endtier, False)
        lines = text.splitlines()
        data = ''
        for line in lines:
            if line.startswith("0x") or line.startswith("0X"):
                data = line
        status=commands.getstatusoutput("bto_advanced_USBIR_cmd -d " + data)
        if status[0] != 0:
            self.textbuffer.set_text(status[1])
        return False


def main():
    ir_remocon_advance()
    gtk.main()

if __name__ == '__main__':
    main()
