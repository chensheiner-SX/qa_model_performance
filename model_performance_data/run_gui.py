import pygame as pg


def load_window(w: int, h: int, active: pg.Color, inactive: pg.Color) -> FeatureScreen:
    """
    loading the elements/feature to the handling class.

    :param w: int - width of the screen
    :param h: int - height of the screen
    :param active: pygame.Color - the color of an active button
    :param inactive: pygame.Color - the color of an inactive button
    :return: features: FeatureScreen - holds all the elements that should be displayed on the screen
    """
    # Define folder load button
    load_file = Button(position=(w * 0.16, h * 0.1), size=(w * 0.22, h * 0.1),
                         color_inactive=inactive, color_active=active, text='Load CSV File',
                         font_size=int(w * 0.03))

    # Define 10 img buttons and their labels
    labels_dict = {}
    img_dict = {}

    button_IP = InputBox(position=(w * 0.2, h * (0.26 + ind * 0.16)), size=(w * 0.3, h * 0.15),
                   color_inactive=inactive, color_active=active, text='172.12.10.28' ,
                   font_size=int(w * 0.025))
    label_IP = Label("NX IP:", w * 0.4, h * (0.26 + ind * 0.16), w * 0.04, h * 0.029, int(w * 0.03))

    sdk_type= DropDown( color_menu, color_option, w * 0.3, h * (0.26 + ind * 0.16), w * 0.3, h * 0.15,
                        'Detector+Tracker',["Detector+Tracker","Detector","Tracker"])
    label_sdk_type = Label("Task:", w * 0.6, h * (0.26 + ind * 0.16), w * 0.04, h * 0.029, int(w * 0.03))


        n = str(ind + 1 + 5)
        img_n = Button(position=(w * 0.8, h * (0.26 + ind * 0.16)), size=(w * 0.3, h * 0.15),
                       color_inactive=inactive, color_active=active, text='img' + n,
                       font_size=int(w * 0.025))
        label_n = Label(" 100", w * 0.53, h * (0.26 + ind * 0.16), w * 0.04, h * 0.029, int(w * 0.03))

        img_dict["img" + n] = img_n
        labels_dict[n] = label_n

    # Define label for final downgrade value
    final_value_label = Label("Final DownGrade Value: ", w * 0.63, h * 0.05, w * 0.1, h * 0.4,
                              int(w * 0.035))

    # Define input box for final DownGrade value
    input_box_dg = InputBox(w * 0.63, h * 0.1, w * 0.01, h * 0.05,
                            text="Enter Value", c_active=active, c_inactive=inactive)

    # Define button for sending the final downgrade value to Slack channel
    sent_button = Button(position=(w * 0.45, h * 0.1), size=(w * 0.3, h * 0.1),
                         color_inactive=inactive, color_active=active, text='Send Slack Message',
                         font_size=int(w * 0.025))
    # Define button for opening manual url
    manual_button = Button(position=(w * 0.025, h * 0.025), size=(w * 0.025, h * 0.025),
                           color_inactive=inactive, color_active=active, text='i',
                           font_size=int(w * 0.025))

    # organizing the screen features in class
    features = FeatureScreen(img_dict, labels_dict, final_value_label, input_box_dg, load_folder, sent_button,
                             manual_button)
    return features
