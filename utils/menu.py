import pystray
from PIL import Image
from pystray import MenuItem

class Menu:
  def __init__(self) -> None:
    pass

  def click_menu(self, icon, item):
    print("点击了", item)


  def on_exit(self,icon, item):
      icon.stop()


  def notify(self, icon: pystray.Icon):
      icon.notify("我是消息类容", "消息标题")

  def run(self):
    self.menu = (
      MenuItem(text='菜单1', action=self.click_menu), 
      MenuItem(text='菜单2', action=self.click_menu),
      MenuItem(text='菜单3', action=self.click_menu, enabled=False),
      MenuItem(text='发送通知', action=self.notify),
      MenuItem(text='我是点击图标的菜单', action=self.click_menu, default=True, visible=False),
      MenuItem(text='退出', action=self.on_exit),
    )
    image = Image.open("logo.ico")
    icon = pystray.Icon("name", image, "鼠标移动到\n托盘图标上\n展示内容", self.menu)
    icon.run()


# if __name__ == '__main__':
#   menu = Menu()
#   menu.run()
    