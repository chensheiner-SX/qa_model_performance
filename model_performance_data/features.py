import pygame as p


class FeatureScreen:
    def __init__(self, label1, label1_loc, label2, label2_loc, drop_down1, drop_down2, undo, reset, pause,
                 load_log_file, replay):
        self.label1 = {"label": label1, "loc": label1_loc}
        self.label2 = {"label": label2, "loc": label2_loc}
        self.labels = {"1": self.label1, "2": self.label2}
        self.drop_down = {"1": drop_down1, "2": drop_down2}
        self.buttons = {"undo": undo, "reset": reset, "pause": pause, 'load_log_file': load_log_file, 'replay': replay}

    def check_drop_menu_down(self):
        result = True
        for drop_menu in self.drop_down.values():
            result = result and not drop_menu.draw_menu
        return result

    def draw(self, screen,if_draw_3_vector=None):
        if if_draw_3_vector==None:
            if_draw_3_vector={"button":True,"label":True,"drop_down":True}

        if if_draw_3_vector["button"]:
            for button in self.buttons.values():
                button.draw(screen)
        if if_draw_3_vector["label"]:
            for label in self.labels.values():
                screen.blit(label["label"], label["loc"])
        if if_draw_3_vector["drop_down"]:
            for drop_menu in self.drop_down.values():
                drop_menu.draw(screen)


class Button:
    def __init__(self, position, size, color_inactive = (100, 100, 100), color_active = None, func = None, text = '',
                 font = "Helvetica",
                 font_size = 16, font_clr = (0, 0, 0)):
        self.clr = color_inactive
        self.size = size
        self.func = func
        self.surf = p.Surface(size)
        self.rect = self.surf.get_rect(center = position)
        self.curclr = ()

        if color_active:
            self.cngclr = color_active
        else:
            self.cngclr = color_inactive

        if len(color_inactive) == 4:
            self.surf.set_alpha(color_inactive[3])

        self.font = p.font.SysFont(font, font_size)
        self.txt = text
        self.font_clr = font_clr
        self.txt_surf = self.font.render(self.txt, True, self.font_clr)
        self.txt_rect = self.txt_surf.get_rect(center = [wh // 2 for wh in self.size])

    def set_text(self, text):
        self.txt = text
        self.txt_surf = self.font.render(self.txt, True, self.font_clr)
        self.txt_rect = self.txt_surf.get_rect(center = [wh // 2 for wh in self.size])

    def draw(self, screen):
        self.mouseover()

        self.surf.fill(self.curclr)
        self.surf.blit(self.txt_surf, self.txt_rect)
        screen.blit(self.surf, self.rect)

    def mouseover(self):
        self.curclr = self.clr
        pos = p.mouse.get_pos()
        if self.rect.collidepoint(pos):
            self.curclr = self.cngclr

    def call_back(self, *args):
        if self.func:
            return self.func(*args)


class DropDown:

    def __init__(self, color_menu, color_option, x, y, w, h, main, options, font = "Helvetica", font_size = 16):
        self.color_menu = color_menu
        self.color_option = color_option
        self.rect = p.Rect(x, y, w, h)
        self.font = pg.font.Font(font, font_size)
        self.main = main
        self.options = options
        self.draw_menu = False
        self.menu_active = False
        self.active_option = -1

    def draw(self, surf):
        p.draw.rect(surf, self.color_menu[self.menu_active], self.rect, 0)
        msg = self.font.render(self.main, 1, (0, 0, 0))
        surf.blit(msg, msg.get_rect(center = self.rect.center))

        if self.draw_menu:
            for i, text in enumerate(self.options):
                rect = self.rect.copy()
                rect.y += (i - 2) * self.rect.height
                rect.x = rect.x - self.rect.width
                p.draw.rect(surf, self.color_option[1 if i == self.active_option else 0], rect, 0)
                msg = self.font.render(text, 1, (0, 0, 0))
                surf.blit(msg, msg.get_rect(center = rect.center))

    def update(self, event_list):
        mouse_pos = p.mouse.get_pos()
        self.menu_active = self.rect.collidepoint(mouse_pos)

        self.active_option = -1
        for i in range(len(self.options)):
            rect = self.rect.copy()
            rect.y += (i - 2) * self.rect.height
            rect.x = rect.x - self.rect.width

            if rect.collidepoint(mouse_pos):
                self.active_option = i
                break

        if not self.menu_active and self.active_option == -1:
            self.draw_menu = False

        for event in event_list:
            if event.type == p.MOUSEBUTTONDOWN:
                if self.menu_active:
                    self.draw_menu = not self.draw_menu
                elif self.draw_menu and self.active_option >= 0:
                    self.draw_menu = False
                    return self.active_option
        return -1
class InputBox:
    def __init__(self,position,size,color_inactive = pg.Color('lightskyblue3'),color_active = pg.Color('dodgerblue2'), text='',
                 font = "Helvetica", font_size = 16):
        self.rect = pg.Rect(position[0], position[1], size[0],size[1])
        self.color_inactive = color_inactive
        self.color_active = color_active
        self.color = self.color_inactive
        self.text = text
        self.font=pg.font.Font(font, font_size)
        self.txt_surface = self.font.render(text, True, self.color)
        self.active = False

    def handle_event(self, event):
        if event.type == pg.MOUSEBUTTONDOWN:
            # If the user clicked on the input_box rect.
            if self.rect.collidepoint(event.pos):
                # Toggle the active variable.
                self.active = not self.active
            else:
                self.active = False
            # Change the current color of the input box.
            self.color = self.color_active if self.active else self.color_inactive
        if event.type == pg.KEYDOWN:
            if self.active:
                if event.key == pg.K_RETURN:
                    print(self.text)
                    self.text = ''
                elif event.key == pg.K_BACKSPACE:
                    self.text = self.text[:-1]
                else:
                    self.text += event.unicode
                # Re-render the text.
                self.txt_surface = self.font.render(self.text, True, self.color)

    def update(self):
        # Resize the box if the text is too long.
        width = max(200, self.txt_surface.get_width()+10)
        self.rect.w = width

    def draw(self, screen):
        # Blit the text.
        screen.blit(self.txt_surface, (self.rect.x+5, self.rect.y+5))
        # Blit the rect.
        pg.draw.rect(screen, self.color, self.rect, 2)