from typing import Union
from PySide6.QtCore import QPropertyAnimation
from PySide6.QtGui import QIcon
from PySide6.QtWidgets import QWidget
from qfluentwidgets import FluentIconBase, FluentWindow, NavigationItemPosition, NavigationTreeWidget
from qfluentwidgets.components.widgets.stacked_widget import PopUpAniInfo


class FluentWindowTwo(FluentWindow):

    def _onCurrentInterfaceChanged(self, index: int):
        widget = self.stackedWidget.widget(index)
        self.navigationInterface.setCurrentItem(widget.objectName())

        self._updateStackedBackground()

    def insertSubInterface(self, index: int, interface: QWidget, icon: Union[FluentIconBase, QIcon, str], text: str,
                        position=NavigationItemPosition.TOP, parent=None, isTransparent=False) -> NavigationTreeWidget:
        """ insert sub interface, the object name of `interface` should be set already
        before calling this method

        need to be completely honest with ya idk why he doesn't have this function in the class

        Parameters
        ----------
        interface: QWidget
            the subinterface to be added

        icon: FluentIconBase | QIcon | str
            the icon of navigation item

        text: str
            the text of navigation item

        position: NavigationItemPosition
            the position of navigation item

        parent: QWidget | str
            * QWidget: the parent of navigation item
            * str: the parent route key of navigation item

        isTransparent: bool
            whether to use transparent background
        """
        if not interface.objectName():
            raise ValueError("The object name of `interface` can't be empty string.")

        parentRouteKey = parent
        if parent and isinstance(parent, QWidget):
            parentRouteKey = parent.objectName()
            if not parentRouteKey:
                raise ValueError("The object name of `parent` can't be empty string.")

        interface.setProperty("isStackedTransparent", isTransparent)
        self.stackedWidget.view.insertWidget(index,interface)
        # i might just 
        self.stackedWidget.view.aniInfos.insert(index, PopUpAniInfo(
            widget=interface,
            deltaX=0,
            deltaY=76,
            ani=QPropertyAnimation(interface, b'pos'),
        ))

        # add navigation item
        routeKey = interface.objectName()
        item = self.navigationInterface.insertItem(
            index=index,
            routeKey=routeKey,
            icon=icon,
            text=text,
            onClick=lambda: self.switchTo(interface),
            position=position,
            tooltip=text,
            parentRouteKey=parentRouteKey # type: ignore
        )

        # initialize selected item
        if self.stackedWidget.count() == 1:
            self.stackedWidget.currentChanged.connect(self._onCurrentInterfaceChanged)
            self.navigationInterface.setCurrentItem(routeKey)
            qrouter.setDefaultRouteKey(self.stackedWidget, routeKey) # type: ignore

        self._updateStackedBackground()

        return item


