#include "precompiled.h"
#include "Common.h"
#include "EditorState.h"
#include "EditorWidgets.h"
#include "WidgetTypes.h"
#include "UndoManager.h"
#include "Base/Main.h"
#include "GroupMessage.h"
#include "CodeGenerator.h"
#include "FileSystemInfo/FileSystemInfo.h"
#include "CommandManager.h"
#include "SettingsManager.h"
#include "WidgetSelectorManager.h"

const float POSITION_CONTROLLER_TIME = 0.5f;
const int HIDE_REMAIN_PIXELS = 3;
const int BAR_HEIGHT = 30;

EditorState::EditorState() :
	mLastClickX(0),
	mLastClickY(0),
	mSelectDepth(0),
	mCurrentWidget(false),
	mRecreate(false),
	mTestMode(false),
	mToolTip(nullptr),
	mPropertiesPanelView(nullptr),
	mSettingsWindow(nullptr),
	mWidgetsWindow(nullptr),
	mMetaSolutionWindow(nullptr),
	mCodeGenerator(nullptr),
	mOpenSaveFileDialog(nullptr),
	mEditorWidgets(nullptr),
	mWidgetTypes(nullptr),
	mUndoManager(nullptr),
	mGroupMessage(nullptr),
	mBar(nullptr),
	mPopupMenuFile(nullptr),
	mPopupMenuWidgets(nullptr),
	mMainMenuControl(nullptr)
{
}

EditorState::~EditorState()
{
}

void EditorState::setupResources()
{
	base::BaseManager::setupResources();
	addResourceLocation(getRootMedia() + "/Tools/LayoutEditor");
	addResourceLocation(getRootMedia() + "/Tools/LayoutEditor/Panels");
	addResourceLocation(getRootMedia() + "/Tools/LayoutEditor/Themes");
	addResourceLocation(getRootMedia() + "/Tools/LayoutEditor/Settings");
	addResourceLocation(getRootMedia() + "/Tools/LayoutEditor/CodeTemplates");
	addResourceLocation(getRootMedia() + "/Common/Wallpapers");
	setResourceFilename("editor.xml");
}
//===================================================================================
void EditorState::createScene()
{
	new tools::SettingsManager();
	tools::SettingsManager::getInstance().initialise();

	new tools::CommandManager();
	tools::CommandManager::getInstance().initialise();

	new tools::WidgetSelectorManager();
	tools::WidgetSelectorManager::getInstance().initialise();

	getStatisticInfo()->setVisible(false);

	// set locale language if it was taken from OS
	if (!mLocale.empty())
		MyGUI::LanguageManager::getInstance().setCurrentLanguage(mLocale);
	// if you want to test LanguageManager uncomment next line
	//MyGUI::LanguageManager::getInstance().setCurrentLanguage("Russian");

	mTestMode = false;

	mWidgetTypes = new WidgetTypes();
	mWidgetTypes->initialise();
	mEditorWidgets = new EditorWidgets();
	mEditorWidgets->initialise();
	mUndoManager = new UndoManager();
	mUndoManager->initialise(mEditorWidgets);
	mGroupMessage = new GroupMessage();

	MyGUI::ResourceManager::getInstance().load("initialise.xml");

	mToolTip = new EditorToolTip();

	mInterfaceWidgets = MyGUI::LayoutManager::getInstance().loadLayout("interface.layout", "LayoutEditor_");

	// settings window
	mSettingsWindow = new SettingsWindow();
	mInterfaceWidgets.push_back(mSettingsWindow->getMainWidget());

	// properties panelView
	mPropertiesPanelView = new PropertiesPanelView();
	mPropertiesPanelView->eventRecreate = MyGUI::newDelegate(this, &EditorState::notifyRecreate);
	mInterfaceWidgets.push_back(mPropertiesPanelView->getMainWidget());

	mWidgetsWindow = new WidgetsWindow();
	mWidgetsWindow->eventToolTip = MyGUI::newDelegate(this, &EditorState::notifyToolTip);
	//mWidgetsWindow->eventSelectWidget = MyGUI::newDelegate(this, &EditorState::notifySelectWidget);
	mInterfaceWidgets.push_back(mWidgetsWindow->getMainWidget());

	mMetaSolutionWindow = new MetaSolutionWindow();
	mMetaSolutionWindow->eventLoadFile = MyGUI::newDelegate(this, &EditorState::saveOrLoadLayoutEvent<false>);
	//mMetaSolutionWindow->eventSelectWidget = MyGUI::newDelegate(this, &EditorState::notifySelectWidget);
	mInterfaceWidgets.push_back(mMetaSolutionWindow->getMainWidget());

	mCodeGenerator = new CodeGenerator();
	mInterfaceWidgets.push_back(mCodeGenerator->getMainWidget());
	mEditorWidgets->setCodeGenerator(mCodeGenerator);

	mOpenSaveFileDialog = new tools::OpenSaveFileDialog();
	mOpenSaveFileDialog->setVisible(false);
	mOpenSaveFileDialog->setFileMask("*.layout");
	mOpenSaveFileDialog->eventEndDialog = MyGUI::newDelegate(this, &EditorState::notifyOpenSaveEndDialog);

	// �������� ����
	createMainMenu();

	mMainMenuControl = new tools::MainMenuControl();

	MyGUI::Widget* widget = mPropertiesPanelView->getMainWidget();
	widget->setCoord(
		widget->getParentSize().width - widget->getSize().width,
		BAR_HEIGHT,
		widget->getSize().width,
		widget->getParentSize().height - BAR_HEIGHT
		);

	// ����� �������� �������� ��������������
	mWidgetsWindow->initialise();

	if (tools::SettingsManager::getInstance().getPropertyValue<bool>("SettingsWindow", "EdgeHide"))
	{
		for (MyGUI::VectorWidgetPtr::iterator iter = mInterfaceWidgets.begin(); iter != mInterfaceWidgets.end(); ++iter)
		{
			MyGUI::ControllerItem* item = MyGUI::ControllerManager::getInstance().createItem(MyGUI::ControllerEdgeHide::getClassTypeName());
			MyGUI::ControllerEdgeHide* controller = item->castType<MyGUI::ControllerEdgeHide>();

			controller->setTime(POSITION_CONTROLLER_TIME);
			controller->setRemainPixels(HIDE_REMAIN_PIXELS);
			controller->setShadowSize(3);

			MyGUI::ControllerManager::getInstance().addItem(*iter, controller);
		}
	}

	clear();

	const tools::VectorUString& additionalPaths = tools::SettingsManager::getInstance().getAdditionalPaths();
	for (tools::VectorUString::const_iterator iter = additionalPaths.begin(); iter != additionalPaths.end(); ++iter)
		addResourceLocation(*iter);

	tools::CommandManager::getInstance().registerCommand("Command_FileLoad", MyGUI::newDelegate(this, &EditorState::commandLoad));
	tools::CommandManager::getInstance().registerCommand("Command_FileSave", MyGUI::newDelegate(this, &EditorState::commandSave));
	tools::CommandManager::getInstance().registerCommand("Command_FileSaveAs", MyGUI::newDelegate(this, &EditorState::commandSaveAs));
	tools::CommandManager::getInstance().registerCommand("Command_ClearAll", MyGUI::newDelegate(this, &EditorState::commandClear));
	tools::CommandManager::getInstance().registerCommand("Command_Test", MyGUI::newDelegate(this, &EditorState::commandTest));
	tools::CommandManager::getInstance().registerCommand("Command_QuitApp", MyGUI::newDelegate(this, &EditorState::commandQuit));
	tools::CommandManager::getInstance().registerCommand("Command_Settings", MyGUI::newDelegate(this, &EditorState::commandSettings));
	tools::CommandManager::getInstance().registerCommand("Command_CodeGenerator", MyGUI::newDelegate(this, &EditorState::commandCodeGenerator));
	tools::CommandManager::getInstance().registerCommand("Command_RecentFiles", MyGUI::newDelegate(this, &EditorState::commandRecentFiles));
	tools::CommandManager::getInstance().registerCommand("WidgetsUpdate", MyGUI::newDelegate(this, &EditorState::commandWidgetsUpdate));

	// ��������� ����� ������� ���� � ��������� ������
	for (std::vector<std::wstring>::iterator iter=mParams.begin(); iter!=mParams.end(); ++iter)
	{
		saveOrLoadLayout(false, false, iter->c_str());
	}

	getGUI()->eventFrameStart += MyGUI::newDelegate(this, &EditorState::notifyFrameStarted);
	tools::SettingsManager::getInstance().eventSettingsChanged += MyGUI::newDelegate(this, &EditorState::notifySettingsChanged);
	tools::WidgetSelectorManager::getInstance().eventChangeSelectedWidget += MyGUI::newDelegate(this, &EditorState::notifyChangeSelectedWidget);
}

void EditorState::destroyScene()
{
	tools::WidgetSelectorManager::getInstance().eventChangeSelectedWidget -= MyGUI::newDelegate(this, &EditorState::notifyChangeSelectedWidget);
	tools::SettingsManager::getInstance().eventSettingsChanged -= MyGUI::newDelegate(this, &EditorState::notifySettingsChanged);
	getGUI()->eventFrameStart -= MyGUI::newDelegate(this, &EditorState::notifyFrameStarted);

	delete mMainMenuControl;
	mMainMenuControl = nullptr;

	delete mPropertiesPanelView;
	mPropertiesPanelView = nullptr;

	delete mGroupMessage;

	mUndoManager->shutdown();
	delete mUndoManager;
	mUndoManager = nullptr;

	mEditorWidgets->shutdown();
	delete mEditorWidgets;
	mEditorWidgets = nullptr;

	mWidgetTypes->shutdown();
	delete mWidgetTypes;
	mWidgetTypes = nullptr;

	delete mToolTip;
	mToolTip = nullptr;

	delete mSettingsWindow;
	mSettingsWindow = nullptr;

	delete mCodeGenerator;
	mCodeGenerator = nullptr;

	delete mMetaSolutionWindow;
	mMetaSolutionWindow = nullptr;

	delete mWidgetsWindow;
	mWidgetsWindow = nullptr;

	delete mOpenSaveFileDialog;
	mOpenSaveFileDialog = nullptr;

	tools::WidgetSelectorManager::getInstance().shutdown();
	delete tools::WidgetSelectorManager::getInstancePtr();

	tools::CommandManager::getInstance().shutdown();
	delete tools::CommandManager::getInstancePtr();

	tools::SettingsManager::getInstance().shutdown();
	delete tools::SettingsManager::getInstancePtr();
}

void EditorState::createMainMenu()
{
	MyGUI::VectorWidgetPtr menu_items = MyGUI::LayoutManager::getInstance().loadLayout("interface_menu.layout");
	MYGUI_ASSERT(menu_items.size() == 1, "Error load main menu");
	mBar = menu_items[0]->castType<MyGUI::MenuBar>();
	mBar->setCoord(0, 0, mBar->getParentSize().width, mBar->getHeight());

	// ������� ����
	MyGUI::MenuItem* menu_file = mBar->getItemById("File");
	mPopupMenuFile = menu_file->getItemChild();

	// ������ ��������� �������� ������
	const tools::VectorUString& recentFiles = tools::SettingsManager::getInstance().getRecentFiles();
	if (!recentFiles.empty())
	{
		MyGUI::MenuItem* menu_item = mPopupMenuFile->getItemById("File/Quit");
		for (tools::VectorUString::const_reverse_iterator iter = recentFiles.rbegin(); iter != recentFiles.rend(); ++iter)
		{
			mPopupMenuFile->insertItem(menu_item, *iter, MyGUI::MenuItemType::Normal, "File/RecentFiles",  *iter);
		}
		// ���� ���� �����, �� ��� ���� ���������
		mPopupMenuFile->insertItem(menu_item, "", MyGUI::MenuItemType::Separator);
	}

	// ���� ��� ��������
	MyGUI::MenuItem* menu_widget = mBar->getItemById("Widgets");
	mPopupMenuWidgets = menu_widget->createItemChild();
	//FIXME
	mPopupMenuWidgets->setPopupAccept(true);
	mPopupMenuWidgets->eventMenuCtrlAccept += MyGUI::newDelegate(this, &EditorState::notifyWidgetsSelect);

	mBar->eventMenuCtrlAccept += newDelegate(this, &EditorState::notifyPopupMenuAccept);

	mInterfaceWidgets.push_back(mBar);
}

void EditorState::notifyPopupMenuAccept(MyGUI::MenuCtrl* _sender, MyGUI::MenuItem* _item)
{
	MyGUI::UString* data = _item->getItemData<MyGUI::UString>(false);
	if (data != nullptr)
		tools::CommandManager::getInstance().setCommandData(*data);

	if (mPopupMenuFile == _item->getMenuCtrlParent())
	{
		std::string id = _item->getItemId();

		if (id == "File/Load")
		{
			tools::CommandManager::getInstance().executeCommand("Command_FileLoad");
		}
		else if (id == "File/Save")
		{
			tools::CommandManager::getInstance().executeCommand("Command_FileSave");
		}
		else if (id == "File/SaveAs")
		{
			tools::CommandManager::getInstance().executeCommand("Command_FileSaveAs");
		}
		else if (id == "File/Clear")
		{
			tools::CommandManager::getInstance().executeCommand("Command_ClearAll");
		}
		else if (id == "File/Settings")
		{
			tools::CommandManager::getInstance().executeCommand("Command_Settings");
		}
		else if (id == "File/Test")
		{
			tools::CommandManager::getInstance().executeCommand("Command_Test");
		}
		else if (id == "File/Code_Generator")
		{
			tools::CommandManager::getInstance().executeCommand("Command_CodeGenerator");
		}
		else if (id == "File/RecentFiles")
		{
			tools::CommandManager::getInstance().executeCommand("Command_RecentFiles");
		}
		else if (id == "File/Quit")
		{
			tools::CommandManager::getInstance().executeCommand("Command_QuitApp");
		}
	}
}
//===================================================================================
void EditorState::injectMouseMove(int _absx, int _absy, int _absz)
{
	if (mTestMode)
	{
		base::BaseManager::injectMouseMove(_absx, _absy, _absz);
		return;
	}

	// drop select depth if we moved mouse
	const int DIST = 2;
	if ((abs(mLastClickX - _absx) > DIST) || (abs(mLastClickY - _absy) > DIST))
	{
		mSelectDepth = 0;
		mLastClickX = _absx;
		mLastClickY = _absy;
	}

	// align to grid if shift not pressed
	int x2, y2;
	if (MyGUI::InputManager::getInstance().isShiftPressed() == false)
	{
		x2 = toGrid(_absx);
		y2 = toGrid(_absy);
	}
	else
	{
		x2 = _absx;
		y2 = _absy;
	}

	mWidgetsWindow->createNewWidget(x2, y2);

	base::BaseManager::injectMouseMove(_absx, _absy, _absz);
}
//===================================================================================
void EditorState::injectMousePress(int _absx, int _absy, MyGUI::MouseButton _id)
{
	if (mTestMode)
	{
		return base::BaseManager::injectMousePress(_absx, _absy, _id);
	}

	if (MyGUI::InputManager::getInstance().isModalAny())
	{
		// if we have modal widgets we can't select any widget
		base::BaseManager::injectMousePress(_absx, _absy, _id);
		return;
	}

	// align to grid if shift not pressed
	int x1, y1;
	if (MyGUI::InputManager::getInstance().isShiftPressed() == false)
	{
		x1 = toGrid(_absx);
		y1 = toGrid(_absy);
	}
	else
	{
		x1 = _absx;
		y1 = _absy;
	}

	// ��������� �����  =)
	mWidgetsWindow->startNewWidget(x1, y1, _id);

	MyGUI::Widget* item = MyGUI::LayerManager::getInstance().getWidgetFromPoint(_absx, _absy);

	// �� ������� ������������� ���� ������ �� ��� ������������
	if (item && (item->getParent() != mPropertiesPanelView->getWidgetRectangle()))
	{
		// ����� ������������� �� �������
		mPropertiesPanelView->getWidgetRectangle()->setVisible(false);
		item = MyGUI::LayerManager::getInstance().getWidgetFromPoint(_absx, _absy);
	}

	if (nullptr != item)
	{
		// find widget registered as container
		while ((nullptr == mEditorWidgets->find(item)) && (nullptr != item)) item = item->getParent();
		MyGUI::Widget* oldItem = item;

		// try to selectin depth
		int depth = mSelectDepth;
		while (depth && (nullptr != item))
		{
			item = item->getParent();
			while ((nullptr == mEditorWidgets->find(item)) && (nullptr != item)) item = item->getParent();
			depth--;
		}
		if (nullptr == item)
		{
			item = oldItem;
			mSelectDepth = 0;
		}

		// found widget
		if (nullptr != item)
		{
			tools::WidgetSelectorManager::getInstance().setSelectedWidget(item);

			if (mWidgetsWindow->getCreatingStatus() != 1)
			{
				//FIXME
				MyGUI::InputManager::getInstance().injectMouseMove(_absx, _absy, 0);// ��� ����� ����� ����� ���� ������
			}
		}
		//FIXME
		MyGUI::InputManager::getInstance().injectMouseRelease(_absx, _absy, _id);
		MyGUI::InputManager::getInstance().injectMousePress(_absx, _absy, _id);
	}
	else
	{
		//FIXME
		MyGUI::InputManager::getInstance().injectMousePress(_absx, _absy, _id);

		tools::WidgetSelectorManager::getInstance().setSelectedWidget(nullptr);
	}

	// ������ �������������
	if (mCurrentWidget && mWidgetsWindow->getCreatingStatus() == 0)
	{
		mPropertiesPanelView->getWidgetRectangle()->setVisible(true);
	}
	else if (mWidgetsWindow->getCreatingStatus())
	{
		mPropertiesPanelView->getWidgetRectangle()->setVisible(false);
	}

	//base::BaseManager::injectMousePress(_absx, _absy, _id);
}
//===================================================================================
void EditorState::injectMouseRelease(int _absx, int _absy, MyGUI::MouseButton _id)
{
	mSelectDepth++;
	if (mTestMode)
	{
		base::BaseManager::injectMouseRelease(_absx, _absy, _id);
		return;
	}

	if (MyGUI::InputManager::getInstance().isModalAny())
	{
	}
	else
	{
		// align to grid if shift not pressed
		int x2, y2;
		if (MyGUI::InputManager::getInstance().isShiftPressed() == false)
		{
			x2 = toGrid(_absx);
			y2 = toGrid(_absy);
		}
		else
		{
			x2 = _absx;
			y2 = _absy;
		}

		mWidgetsWindow->finishNewWidget(x2, y2);
	}

	mUndoManager->dropLastProperty();

	base::BaseManager::injectMouseRelease(_absx, _absy, _id);
}
//===================================================================================
void EditorState::injectKeyPress(MyGUI::KeyCode _key, MyGUI::Char _text)
{
	MyGUI::InputManager& input = MyGUI::InputManager::getInstance();

	if (mTestMode)
	{
		if (input.isModalAny() == false)
		{
			if (_key == MyGUI::KeyCode::Escape)
			{
				notifyEndTest();
			}
		}

		MyGUI::InputManager::getInstance().injectKeyPress(_key, _text);
		//base::BaseManager::injectKeyPress(_key, _text);
		return;
	}

	if (tools::Dialog::getAnyDialog())
	{
		if (_key == MyGUI::KeyCode::Escape)
			tools::Dialog::endTopDialog(false);
		else if (_key == MyGUI::KeyCode::Return)
			tools::Dialog::endTopDialog(true);
	}
	else
	{
		if (_key == MyGUI::KeyCode::Escape)
		{
			notifyQuit();
			return;
		}

		if (input.isControlPressed())
		{
			if (_key == MyGUI::KeyCode::O
				|| _key == MyGUI::KeyCode::L)
				tools::CommandManager::getInstance().executeCommand("Command_FileLoad");
			else if (_key == MyGUI::KeyCode::S)
			tools::CommandManager::getInstance().executeCommand("Command_FileSave");
			else if (_key == MyGUI::KeyCode::Z)
			{
				mUndoManager->undo();
				tools::WidgetSelectorManager::getInstance().setSelectedWidget(nullptr);
			}
			else if ((_key == MyGUI::KeyCode::Y) || ((input.isShiftPressed()) && (_key == MyGUI::KeyCode::Z)))
			{
				mUndoManager->redo();
				tools::WidgetSelectorManager::getInstance().setSelectedWidget(nullptr);
			}
			else if (_key == MyGUI::KeyCode::T)
			{
				notifyTest();
				return;
			}
			else if (_key == MyGUI::KeyCode::R)
			{
				mPropertiesPanelView->toggleRelativeMode();
				return;
			}
			else if (_key == MyGUI::KeyCode::F11)
			{
				getStatisticInfo()->setVisible(!getStatisticInfo()->getVisible());
				return;
			}
			else if (_key == MyGUI::KeyCode::F12)
			{
				getFocusInput()->setFocusVisible(!getFocusInput()->getFocusVisible());
				return;
			}
		}
	}

	MyGUI::InputManager::getInstance().injectKeyPress(_key, _text);
	//base::BaseManager::injectKeyPress(_key, _text);
}
//===================================================================================
void EditorState::injectKeyRelease(MyGUI::KeyCode _key)
{
	if (mTestMode)
	{
		return base::BaseManager::injectKeyRelease(_key);
	}

	return base::BaseManager::injectKeyRelease(_key);
}
//===================================================================================
void EditorState::notifyFrameStarted(float _time)
{
	GroupMessage::getInstance().showMessages();

	if (mRecreate)
	{
		mRecreate = false;
		// ������ ������������, ������ ����� ������� ��� ������ :)
		tools::WidgetSelectorManager::getInstance().setSelectedWidget(nullptr);
	}
}

void EditorState::commandWidgetsUpdate(const MyGUI::UString& _commandName)
{
	solutionUpdate();
	widgetsUpdate();
}

void EditorState::notifySettingsChanged(const MyGUI::UString& _sectionName, const MyGUI::UString& _propertyName)
{
	if (_sectionName == "SettingsWindow")
	{
		solutionUpdate();
		widgetsUpdate();
	}
}

void EditorState::notifyLoad()
{
	if (mUndoManager->isUnsaved())
	{
		MyGUI::Message* message = MyGUI::Message::createMessageBox(
			"Message",
			localise("Warning"),
			localise("Warn_unsaved_data"),
			MyGUI::MessageBoxStyle::IconWarning |
			MyGUI::MessageBoxStyle::Yes | MyGUI::MessageBoxStyle::No | MyGUI::MessageBoxStyle::Cancel
			);
		message->eventMessageBoxResult += newDelegate(this, &EditorState::notifyConfirmLoadMessage);
		message->setUserString("FileName", mFileName);
		return;
	}

	setModeSaveLoadDialog(false, mFileName);
}

bool EditorState::notifySave()
{
	if (mFileName != "")
	{
		return saveOrLoadLayout(true, false, mFileName);
	}
	else
	{
		setModeSaveLoadDialog(true, mFileName);
		return false;
	}
}

void EditorState::notifySettings()
{
	mSettingsWindow->setVisible(true);
	MyGUI::LayerManager::getInstance().upLayerItem(mSettingsWindow->getMainWidget());
}

void EditorState::notifyTest()
{
	mTestLayout = mEditorWidgets->savexmlDocument();
	mEditorWidgets->clear();
	tools::WidgetSelectorManager::getInstance().setSelectedWidget(nullptr);

	for (MyGUI::VectorWidgetPtr::iterator iter = mInterfaceWidgets.begin(); iter != mInterfaceWidgets.end(); ++iter)
	{
		if ((*iter)->getVisible())
		{
			(*iter)->setUserString("WasVisible", "true");
			(*iter)->setVisible(false);
		}
	}

	mEditorWidgets->loadxmlDocument(mTestLayout, true);
	mTestMode = true;
}

void EditorState::notifyEndTest()
{
	for (MyGUI::VectorWidgetPtr::iterator iter = mInterfaceWidgets.begin(); iter != mInterfaceWidgets.end(); ++iter)
	{
		if ((*iter)->getUserString("WasVisible") == "true")
		{
			(*iter)->setVisible(true);
		}
	}
	mTestMode = false;
	clear(false);
	mEditorWidgets->loadxmlDocument(mTestLayout);
}

void EditorState::notifyClear()
{
	MyGUI::Message* message = MyGUI::Message::createMessageBox(
		"Message",
		localise("Warning"),
		localise("Warn_delete_all_widgets"),
		MyGUI::MessageBoxStyle::IconWarning | MyGUI::MessageBoxStyle::Yes | MyGUI::MessageBoxStyle::No
		);
	message->eventMessageBoxResult += newDelegate(this, &EditorState::notifyClearMessage);
}

void EditorState::notifyClearMessage(MyGUI::Message* _sender, MyGUI::MessageBoxStyle _result)
{
	if (_result == MyGUI::MessageBoxStyle::Yes || _result == MyGUI::MessageBoxStyle::Button1)
	{
		clear();
	}
}

void EditorState::clear(bool _clearName)
{
	mWidgetsWindow->clearNewWidget();
	mRecreate = false;
	if (_clearName)
		mFileName = "";
	mTestMode = false;
	mEditorWidgets->clear();

	tools::WidgetSelectorManager::getInstance().setSelectedWidget(nullptr);

	mUndoManager->shutdown();
	mUndoManager->initialise(mEditorWidgets);
	mSelectDepth = 0;

	if (_clearName)
		setWindowCaption(L"MyGUI Layout Editor");
}

void EditorState::notifyQuit()
{
	if (mUndoManager->isUnsaved())
	{
		MyGUI::Message* message = MyGUI::Message::createMessageBox(
			"Message",
			localise("Warning"),
			localise("Warn_unsaved_data"),
			MyGUI::MessageBoxStyle::IconWarning |
			MyGUI::MessageBoxStyle::Yes | MyGUI::MessageBoxStyle::No | MyGUI::MessageBoxStyle::Cancel
			);
		message->eventMessageBoxResult += newDelegate(this, &EditorState::notifyConfirmQuitMessage);
		message->setUserString("FileName", mFileName);
		return;
	}

	// �������
	quit();
}

void EditorState::notifyConfirmQuitMessage(MyGUI::Message* _sender, MyGUI::MessageBoxStyle _result)
{
	if ( _result == MyGUI::MessageBoxStyle::Yes )
	{
		if (notifySave())
		{
			// �������
			quit();
		}
	}
	else if ( _result == MyGUI::MessageBoxStyle::No )
	{
		// �������
		quit();
	}
	/*else if ( _result == MyGUI::MessageBoxStyle::Cancel )
	{
		// do nothing
	}
	*/
}

bool EditorState::isMetaSolution(const MyGUI::UString& _fileName)
{
	MyGUI::xml::Document doc;
	if (!doc.open(_fileName))
	{
		return false;
	}

	MyGUI::xml::ElementPtr root = doc.getRoot();
	if (!root || (root->getName() != "MyGUI"))
	{
		return false;
	}

	std::string type;
	if (root->findAttribute("type", type))
	{
		if (type == "MetaSolution")
		{
			return true;
		}
	}

	return false;
}

void EditorState::clearWidgetWindow()
{
	WidgetTypes::getInstance().clearAllSkins();
	mWidgetsWindow->clearAllSheets();
}

void EditorState::loadFile(const std::wstring& _file)
{
	// ���� �������, �� �������
	bool solution = isMetaSolution(MyGUI::UString(_file).asUTF8_c_str());
	if (solution)
	{
		clearWidgetWindow();
	}

	if (!saveOrLoadLayout(false, true, MyGUI::UString(_file).asUTF8_c_str()))
	{
		MyGUI::ResourceManager::getInstance().load(MyGUI::UString(_file).asUTF8_c_str()/*, ""*/);
	}

	if (solution)
	{
		this->mWidgetsWindow->initialise();
	}
}

void EditorState::notifyConfirmLoadMessage(MyGUI::Message* _sender, MyGUI::MessageBoxStyle _result)
{
	if ( _result == MyGUI::MessageBoxStyle::Yes )
	{
		if (notifySave())
		{
			setModeSaveLoadDialog(false, mFileName);
		}
	}
	else if ( _result == MyGUI::MessageBoxStyle::No )
	{
		setModeSaveLoadDialog(false, mFileName);
	}
	/*else if ( _result == MyGUI::MessageBoxStyle::Cancel )
	{
		// do nothing
	}
	*/
}

void EditorState::solutionUpdate()
{
	if (mMetaSolutionWindow->getVisible())
		mMetaSolutionWindow->updateList();
}

void EditorState::widgetsUpdate()
{
	bool print_name = tools::SettingsManager::getInstance().getPropertyValue<bool>("SettingsWindow", "ShowName");
	bool print_type = tools::SettingsManager::getInstance().getPropertyValue<bool>("SettingsWindow", "ShowType");
	bool print_skin = tools::SettingsManager::getInstance().getPropertyValue<bool>("SettingsWindow", "ShowSkin");

	mPopupMenuWidgets->removeAllItems();

	for (std::vector<WidgetContainer*>::iterator iter = mEditorWidgets->widgets.begin(); iter != mEditorWidgets->widgets.end(); ++iter )
	{
		createWidgetPopup(*iter, mPopupMenuWidgets, print_name, print_type, print_skin);
	}
}

void EditorState::createWidgetPopup(WidgetContainer* _container, MyGUI::MenuCtrl* _parentPopup, bool _print_name, bool _print_type, bool _print_skin)
{
	bool submenu = !_container->childContainers.empty();

	_parentPopup->addItem(getDescriptionString(_container->widget, _print_name, _print_type, _print_skin), submenu ? MyGUI::MenuItemType::Popup : MyGUI::MenuItemType::Normal);
	_parentPopup->setItemDataAt(_parentPopup->getItemCount()-1, _container->widget);

	if (submenu)
	{
		MyGUI::MenuCtrl* child = _parentPopup->createItemChildAt(_parentPopup->getItemCount()-1);
		child->eventMenuCtrlAccept += MyGUI::newDelegate(this, &EditorState::notifyWidgetsSelect);
		child->setPopupAccept(true);

		for (std::vector<WidgetContainer*>::iterator iter = _container->childContainers.begin(); iter != _container->childContainers.end(); ++iter )
		{
			createWidgetPopup(*iter, child, _print_name, _print_type, _print_skin);
		}
	}
}

void EditorState::notifyWidgetsSelect(MyGUI::MenuCtrl* _sender, MyGUI::MenuItem* _item)
{
	MyGUI::Widget* widget = *_item->getItemData<MyGUI::Widget*>();
	tools::WidgetSelectorManager::getInstance().setSelectedWidget(widget);
}

std::string EditorState::getDescriptionString(MyGUI::Widget* _widget, bool _print_name, bool _print_type, bool _print_skin)
{
	std::string name = "";
	std::string type = "";
	std::string skin = "";

	WidgetContainer * widgetContainer = mEditorWidgets->find(_widget);
	if (_print_name)
	{
		if (widgetContainer->name.empty())
		{
			// trim "LayoutEditorWidget_"
			/*name = _widget->getName();
			if (0 == strncmp("LayoutEditorWidget_", name.c_str(), 19))
			{
					std::string::iterator iter = std::find(name.begin(), name.end(), '_');
					if (iter != name.end()) name.erase(name.begin(), ++iter);
					name = "#{ColourMenuName}" + name;
			}
			name = "#{ColourMenuName}[" + name + "] ";*/
		}
		else
		{
			// FIXME my.name ��� ����� ��� ������ ��� ������ ������� � ������
			name = "#{ColourMenuName}'" + widgetContainer->name + "' ";
		}
	}

	if (_print_type)
	{
		// FIXME my.name ��� ����� ��� ������ ��� ������ ������� � ������
		type = "#{ColourMenuType}[" + _widget->getTypeName() + "] ";
	}

	if (_print_skin)
	{
		// FIXME my.name ��� ����� ��� ������ ��� ������ ������� � ������
		skin = "#{ColourMenuSkin}" + widgetContainer->skin + " ";
	}
	return MyGUI::LanguageManager::getInstance().replaceTags(type + skin + name);
}

void EditorState::notifyToolTip(MyGUI::Widget* _sender, const MyGUI::ToolTipInfo & _info)
{
	if (_info.type == MyGUI::ToolTipInfo::Show)
	{
		mToolTip->show(_sender);
		mToolTip->move(_info.point);
	}
	else if (_info.type == MyGUI::ToolTipInfo::Hide)
	{
		mToolTip->hide();
	}
	else if (_info.type == MyGUI::ToolTipInfo::Move)
	{
		mToolTip->move(_info.point);
	}
}

void EditorState::notifyOpenSaveEndDialog(tools::Dialog* _dialog, bool _result)
{
	if (_result)
	{
		MyGUI::UString file = common::concatenatePath(mOpenSaveFileDialog->getCurrentFolder(), mOpenSaveFileDialog->getFileName());
		bool save = mOpenSaveFileDialog->getMode() == "Save";
		saveOrLoadLayout(save, false, file);
	}
	else
	{
		mOpenSaveFileDialog->setVisible(false);
	}
}

void EditorState::setModeSaveLoadDialog(bool _save, const MyGUI::UString& _filename)
{
	if (_save)
		mOpenSaveFileDialog->setDialogInfo(localise("Save"), localise("Save"));
	else
		mOpenSaveFileDialog->setDialogInfo(localise("Load"), localise("Load"));

	size_t pos = _filename.find_last_of(L"\\/");
	if (pos == MyGUI::UString::npos)
	{
		mOpenSaveFileDialog->setFileName(_filename);
	}
	else
	{
		mOpenSaveFileDialog->setCurrentFolder(_filename.substr(0, pos));
		mOpenSaveFileDialog->setFileName(_filename.substr(pos + 1));
	}

	mOpenSaveFileDialog->setVisible(true);
	mOpenSaveFileDialog->setMode(_save ? "Save" : "Load");
}

bool EditorState::saveOrLoadLayout(bool Save, bool Silent, const MyGUI::UString& _file)
{
	if (!Save) clear();

	if ( (Save && mEditorWidgets->save(_file)) ||
		(!Save && mEditorWidgets->load(_file)) )
	{
		mFileName = _file;
		setWindowCaption(_file.asWStr() + L" - MyGUI Layout Editor");
		tools::SettingsManager::getInstance().addRecentFile(_file);

		mOpenSaveFileDialog->setVisible(false);

		mUndoManager->addValue();
		mUndoManager->setUnsaved(false);
		return true;
	}
	else if (!Silent)
	{
		std::string saveLoad = Save ? localise("Save") : localise("Load");
		MyGUI::Message* message = MyGUI::Message::createMessageBox(
			"Message",
			localise("Warning"),
			"Failed to " + saveLoad + " file '" + _file + "'",
			MyGUI::MessageBoxStyle::IconWarning | MyGUI::MessageBoxStyle::Ok
			);
	}

	return false;
}

void EditorState::prepare()
{
	// ������������� ������ �� ���������� ���������
	// ��� ����� �� ����� ����������� ���� �����
	mLocale = ::setlocale( LC_ALL, "" );
	// erase everything after '_' to get language name
	mLocale.erase(std::find(mLocale.begin(), mLocale.end(), '_'), mLocale.end());
	if (mLocale == "ru") mLocale = "Russian";
	else if (mLocale == "en") mLocale = "English";

#if OGRE_PLATFORM == OGRE_PLATFORM_WIN32

	// ��� ����� ���� ����� ���� ������� � ����� �����������
	wchar_t buff[MAX_PATH];
	::GetModuleFileNameW(0, buff, MAX_PATH);

	std::wstring dir = buff;
	size_t pos = dir.find_last_of(L"\\/");
	if (pos != dir.npos)
	{
		// ������������� ���������� �����������
		::SetCurrentDirectoryW(dir.substr(0, pos+1).c_str());
	}

	// ����� ����� ��������� �������, ����������
	//��������� � ��������� ����� �� �������������
	std::wifstream stream;
	std::wstring tmp;
	std::wstring delims = L" ";
	std::wstring source = GetCommandLineW();
	size_t start = source.find_first_not_of(delims);
	while (start != source.npos)
	{
		size_t end = source.find_first_of(delims, start);
		if (end != source.npos)
		{
			tmp += source.substr(start, end-start);

			// ����� ����� ���� � ��������
			if (tmp.size() > 2)
			{
				if ((tmp[0] == L'"') && (tmp[tmp.size()-1] == L'"'))
				{
					tmp = tmp.substr(1, tmp.size()-2);
				}
			}

			stream.open(tmp.c_str());
			if (stream.is_open())
			{
				if (tmp.size() > 4 && tmp.substr(tmp.size() - 4) != L".exe")
					mParams.push_back(tmp);

				tmp.clear();
				stream.close();
			}
			else
				tmp += delims;
		}
		else
		{
			tmp += source.substr(start);

			// ����� ����� ���� � ��������
			if (tmp.size() > 2)
			{
				if ((tmp[0] == L'"') && (tmp[tmp.size()-1] == L'"'))
				{
					tmp = tmp.substr(1, tmp.size()-2);
				}
			}

			stream.open(tmp.c_str());
			if (stream.is_open())
			{
				if (tmp.size() > 4 && tmp.substr(tmp.size() - 4) != L".exe")
					mParams.push_back(tmp);

				tmp.clear();
				stream.close();
			}
			else
				tmp += delims;
			break;
		}
		start = source.find_first_not_of(delims, end + 1);
	};

#else
#endif
}

void EditorState::onFileDrop(const std::wstring& _filename)
{
	saveOrLoadLayout(false, false, _filename);
}

bool EditorState::onWinodwClose(size_t _handle)
{
#if OGRE_PLATFORM == OGRE_PLATFORM_WIN32
	if (::IsIconic((HWND)_handle))
		ShowWindow((HWND)_handle, SW_SHOWNORMAL);
#endif

	notifyQuit();
	return false;
}

int EditorState::toGrid(int _x)
{
	return _x / grid_step * grid_step;
}

void EditorState::notifyRecreate()
{
	mRecreate = true;
}

void EditorState::commandLoad(const MyGUI::UString& _commandName)
{
	notifyLoad();
}

void EditorState::commandSave(const MyGUI::UString& _commandName)
{
	notifySave();
}

void EditorState::commandSaveAs(const MyGUI::UString& _commandName)
{
	setModeSaveLoadDialog(true, mFileName);
}

void EditorState::commandClear(const MyGUI::UString& _commandName)
{
	notifyClear();
}

void EditorState::commandTest(const MyGUI::UString& _commandName)
{
	notifyTest();
}

void EditorState::commandQuit(const MyGUI::UString& _commandName)
{
	notifyQuit();
}

void EditorState::commandSettings(const MyGUI::UString& _commandName)
{
	notifySettings();
}

void EditorState::commandCodeGenerator(const MyGUI::UString& _commandName)
{
	mCodeGenerator->getMainWidget()->setVisible(true);
}

void EditorState::commandRecentFiles(const MyGUI::UString& _commandName)
{
	saveOrLoadLayout(false, false, tools::CommandManager::getInstance().getCommandData());
}

void EditorState::notifyChangeSelectedWidget(MyGUI::Widget* _current_widget)
{
	if (_current_widget == mCurrentWidget)
	{
		if (mCurrentWidget != nullptr)
		{
			mPropertiesPanelView->getWidgetRectangle()->setVisible(true);
			MyGUI::InputManager::getInstance().setKeyFocusWidget(mPropertiesPanelView->getWidgetRectangle());
		}
		return;
	}

	mCurrentWidget = _current_widget;
}

MYGUI_APP(EditorState)
