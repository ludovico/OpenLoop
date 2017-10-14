import sublime
import sublime_plugin
import socket

class sendcodeCommand(sublime_plugin.TextCommand):
	def run(self, edit):
		first_selection_region = self.view.sel()[0]
		if first_selection_region.size() == 0:	
			text = self.view.substr(self.view.line(first_selection_region))
		else:
			text = self.view.substr(first_selection_region)
		b = bytes(text, "UTF-8")
		s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
		s.connect(("127.0.0.1", 8989))
		s.send((len(b)).to_bytes(4, byteorder="little"))
		s.send(b)
		s.close()
