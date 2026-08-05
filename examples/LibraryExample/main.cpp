#ifdef QT_ENABLED
#include <QApplication>
#endif
#include <iostream>
#include <cstdlib>
#include <vector>
#include <memory>
#include "TaskGraph.h"
#include "Logger.h"
#include "CrashReport.h"

#ifdef QT_WIDGETS_ENABLED
#include <QWidget>
#include <QMainWindow>
#include <QToolBar>
#include <QAction>
#include <QDockWidget>
#include <QComboBox>
#include <QCheckBox>
#include <QFormLayout>
#include <QScrollArea>
#include <QColorDialog>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include "gui/TaskGraphWidget.h"
#include "gui/GraphVisualConfig.h"
#endif


#ifdef QT_WIDGETS_ENABLED
// Dockable panel that live-edits a GraphVisualConfig and pushes every change
// to the hosted TaskGraphWidget. Demo code: plain QWidget + lambdas, no moc.
class VisualConfigPanel : public QWidget
{
public:
	using GVC = TaskGraph::Gui::GraphVisualConfig;

	VisualConfigPanel(TaskGraph::Gui::TaskGraphWidget* target, QWidget* parent)
		: QWidget(parent)
		, m_target(target)
		, m_cfg(target->visualConfig())
	{
		auto* outer = new QVBoxLayout(this);

		m_preset = new QComboBox(this);
		m_preset->addItem("Light");
		m_preset->addItem("Dark");

		m_mode = new QComboBox(this);
		m_mode->addItem("Two edges (left+right)");
		m_mode->addItem("One center");

		m_leftExact   = new QCheckBox("Left waypoint exact", this);
		m_rightExact  = new QCheckBox("Right waypoint exact", this);
		m_centerExact = new QCheckBox("Center waypoint exact", this);
		m_portExact   = new QCheckBox("Port anchor exact", this);

		auto* form = new QFormLayout();
		form->addRow("Preset", m_preset);
		form->addRow("Waypoint mode", m_mode);
		form->addRow(m_leftExact);
		form->addRow(m_rightExact);
		form->addRow(m_centerExact);
		form->addRow(m_portExact);

		auto* colorForm = new QFormLayout();
		addColorRow(colorForm, "Background", &GVC::background);
		addColorRow(colorForm, "Node border", &GVC::nodeBorder);
		addColorRow(colorForm, "Node text", &GVC::nodeText);
		addColorRow(colorForm, "Status: Pending", &GVC::statusPending);
		addColorRow(colorForm, "Status: Ready", &GVC::statusReady);
		addColorRow(colorForm, "Status: Running", &GVC::statusRunning);
		addColorRow(colorForm, "Status: Done", &GVC::statusDone);
		addColorRow(colorForm, "Status: Failed", &GVC::statusFailed);
		addColorRow(colorForm, "Status: Cancelled", &GVC::statusCancelled);
		addColorRow(colorForm, "Status: Skipped", &GVC::statusSkipped);
		addColorRow(colorForm, "Edge line", &GVC::edgeLine);
		addColorRow(colorForm, "Edge arrow", &GVC::edgeArrow);
		addColorRow(colorForm, "Highlight: Incoming", &GVC::edgeHighlightIncoming);
		addColorRow(colorForm, "Highlight: Outgoing", &GVC::edgeHighlightOutgoing);
		addColorRow(colorForm, "Debug waypoint", &GVC::debugWaypoint);
		addColorRow(colorForm, "Debug anchor", &GVC::debugAnchor);

		auto* colorHost = new QWidget(this);
		colorHost->setLayout(colorForm);
		auto* scroll = new QScrollArea(this);
		scroll->setWidget(colorHost);
		scroll->setWidgetResizable(true);
		scroll->setMinimumHeight(300);

		outer->addLayout(form);
		outer->addWidget(new QLabel("Colors:", this));
		outer->addWidget(scroll, 1);

		connect(m_preset, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
			this, [this](int idx)
		{
			m_cfg = (idx == 1) ? GVC::dark() : GVC::light();
			refreshControls();
			apply();
		});
		connect(m_mode, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
			this, [this](int idx)
		{
			m_cfg.waypointMode = (idx == 1)
				? TaskGraph::Gui::ColumnWaypointMode::OneCenter
				: TaskGraph::Gui::ColumnWaypointMode::TwoEdges;
			apply();
		});
		connect(m_leftExact, &QCheckBox::toggled, this,
			[this](bool v) { m_cfg.leftWaypointExact = v; apply(); });
		connect(m_rightExact, &QCheckBox::toggled, this,
			[this](bool v) { m_cfg.rightWaypointExact = v; apply(); });
		connect(m_centerExact, &QCheckBox::toggled, this,
			[this](bool v) { m_cfg.centerWaypointExact = v; apply(); });
		connect(m_portExact, &QCheckBox::toggled, this,
			[this](bool v) { m_cfg.portAnchorExact = v; apply(); });

		refreshControls();
	}

private:
	struct Swatch { QPushButton* button; QColor GVC::* field; };

	void apply()
	{
		m_target->setVisualConfig(m_cfg);
	}

	void updateSwatch(QPushButton* b, const QColor& c)
	{
		b->setText(c.name());
		b->setStyleSheet(QString("background-color: %1; color: %2; border: 1px solid #888;")
			.arg(c.name(), c.lightness() < 128 ? "#ffffff" : "#000000"));
	}

	void addColorRow(QFormLayout* form, const QString& label, QColor GVC::* field)
	{
		auto* btn = new QPushButton(this);
		btn->setMinimumWidth(120);
		updateSwatch(btn, m_cfg.*field);
		connect(btn, &QPushButton::clicked, this, [this, btn, field]()
		{
			QColor picked = QColorDialog::getColor(m_cfg.*field, this, "Select color");
			if (picked.isValid())
			{
				m_cfg.*field = picked;
				updateSwatch(btn, picked);
				apply();
			}
		});
		form->addRow(label, btn);
		m_swatches.push_back({ btn, field });
	}

	void refreshControls()
	{
		m_mode->blockSignals(true);
		m_mode->setCurrentIndex(
			m_cfg.waypointMode == TaskGraph::Gui::ColumnWaypointMode::OneCenter ? 1 : 0);
		m_mode->blockSignals(false);

		auto setChecked = [](QCheckBox* box, bool v)
		{
			box->blockSignals(true);
			box->setChecked(v);
			box->blockSignals(false);
		};
		setChecked(m_leftExact, m_cfg.leftWaypointExact);
		setChecked(m_rightExact, m_cfg.rightWaypointExact);
		setChecked(m_centerExact, m_cfg.centerWaypointExact);
		setChecked(m_portExact, m_cfg.portAnchorExact);

		for (const Swatch& s : m_swatches)
			updateSwatch(s.button, m_cfg.*(s.field));
	}

	TaskGraph::Gui::TaskGraphWidget* m_target;
	GVC m_cfg;
	QComboBox* m_preset;
	QComboBox* m_mode;
	QCheckBox* m_leftExact;
	QCheckBox* m_rightExact;
	QCheckBox* m_centerExact;
	QCheckBox* m_portExact;
	std::vector<Swatch> m_swatches;
};
#endif


class TestTask : public TaskGraph::Task
{
	public:
	TestTask(const std::string& name)
		: TaskGraph::Task(name)
	{
	}
	private:
	void work(TaskGraph::TaskContext& ctx) override
	{
		logger().logInfo(getName() + " started");
		int steps = 4;
		for (int i = 0; i < steps; ++i)
		{
			if (ctx.isCancelRequested())
			{
				logger().logWarning(getName() + " cancelled");
				return;
			}
			logger().logInfo(getName() + " step " + std::to_string(i + 1) + "/" + std::to_string(steps));
			if (i == 1)
				logger().logWarning(getName() + " taking longer than expected");
			if (i == 2)
				logger().logError(getName() + " retrying step " + std::to_string(i + 1));
			logger().logDebug(getName() + " internal state OK");
			std::this_thread::sleep_for(std::chrono::milliseconds(500));
		}
		logger().logInfo(getName() + " done");
	}
};

class AskGuiTask : public TaskGraph::Task
{
public:
	AskGuiTask(const std::string& name)
		: TaskGraph::Task(name)
	{
	}
private:
	void work(TaskGraph::TaskContext& ctx) override
	{
		logger().logInfo(getName() + " requesting GUI input...");
		QVariant response = ctx.askGui(QVariant("What value should " + QString::fromStdString(getName()) + " use?"));
		if (!response.isValid())
		{
			logger().logWarning(getName() + " got no response (cancelled?)");
			return;
		}
		std::string val = response.toString().toStdString();
		logger().logInfo(getName() + " got response: " + val);
		std::this_thread::sleep_for(std::chrono::milliseconds(500));
		logger().logInfo(getName() + " done with value: " + val);
	}
};

void exiting();
void exceptionCallback();
int main(int argc, char* argv[])
{
	std::atexit(exiting);

	CrashReport::Profiler::start();
	CrashReport::LibraryInfo::printInfo();
	CrashReport::ExceptionHandler::setup("crashFiles");
	//CrashReport::ExceptionHandler::setExceptionCallback(exceptionCallback);
#ifdef QT_WIDGETS_ENABLED
	QGuiApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
	QGuiApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
	QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
#endif
#ifdef QT_ENABLED
	QApplication app(argc, argv);
#endif
	TaskGraph::Profiler::start();
	TaskGraph::LibraryInfo::printInfo();
/*#ifdef QT_WIDGETS_ENABLED
	QWidget* widget = TaskGraph::LibraryInfo::createInfoWidget();
	if (widget)
		widget->show();
#endif*/

	Log::UI::NativeConsoleView consoleView;

	unsigned int hwThreads = std::thread::hardware_concurrency();
	TaskGraph::TaskScheduler scheduler(hwThreads > 0 ? hwThreads : 8);

	// Large multi-layer graph (~36 tasks across 8 layers) to stress layout and
	// skip-layer edge routing. Every non-root task takes one adjacent (previous
	// layer) dependency to fix its layer, plus several skip dependencies that
	// reach back 2-5 layers.
	std::vector<std::shared_ptr<TestTask>> t;
	for (int i = 1; i <= 36; ++i)
	{
		auto task = std::make_shared<TestTask>("Task" + std::to_string(i));
		scheduler.addTask(task);
		t.push_back(task);
	}
	auto askTask = std::make_shared<AskGuiTask>(std::string("AskGuiTask"));
	scheduler.addTask(askTask);

	// 1-based accessor: T(n) is "Taskn"
	auto T = [&](int n) -> std::shared_ptr<TaskGraph::Task> { return t[n - 1]; };

	// give some tasks non-default config for inspector demo
	t[0]->setWeight(2.0f);                                   // Task1
	t[2]->setTimeout(std::chrono::milliseconds(10000));      // Task3
	t[4]->setMaxRetries(3);                                  // Task5
	t[4]->setRetryBackoff(std::chrono::milliseconds(200));
	t[6]->setWeight(0.5f);                                   // Task7
	t[19]->setWeight(3.0f);                                  // Task20
	t[29]->setTimeout(std::chrono::milliseconds(8000));      // Task30

	// Layers: L0 [1-5] L1 [6-10] L2 [11-15] L3 [16-20]
	//         L4 [21-25] L5 [26-29] L6 [30-33] L7 [34-36]

	// L1 (adjacent to L0)
	T(6)->addDependency(T(1));
	T(7)->addDependency(T(2));
	T(8)->addDependency(T(3));
	T(9)->addDependency(T(4));
	T(10)->addDependency(T(5));

	// L2
	T(11)->addDependency(T(6));  T(11)->addDependency(T(1));   // skip 2
	T(12)->addDependency(T(7));  T(12)->addDependency(T(2));   // skip 2
	T(13)->addDependency(T(8));
	T(14)->addDependency(T(9));  T(14)->addDependency(T(3));   // skip 2
	T(15)->addDependency(T(10));

	// L3
	T(16)->addDependency(T(11)); T(16)->addDependency(T(6)); T(16)->addDependency(T(1)); // skip 2,3
	T(17)->addDependency(T(12)); T(17)->addDependency(T(8));   // skip 2
	T(18)->addDependency(T(13)); T(18)->addDependency(T(2));   // skip 3
	T(19)->addDependency(T(14));
	T(20)->addDependency(T(15)); T(20)->addDependency(T(9));   // skip 2

	// askTask sits early (L1) and is reached later mid-graph
	askTask->addDependency(T(3));

	// L4
	T(21)->addDependency(T(16)); T(21)->addDependency(T(11)); T(21)->addDependency(T(6)); // skip 2,3
	T(22)->addDependency(T(17)); T(22)->addDependency(T(7));   // skip 3
	T(23)->addDependency(T(18)); T(23)->addDependency(T(13));  // skip 2
	T(24)->addDependency(T(19)); T(24)->addDependency(askTask); T(24)->addDependency(T(1)); // askTask skip 3, T1 skip 4
	T(25)->addDependency(T(20)); T(25)->addDependency(T(10));  // skip 3

	// L5
	T(26)->addDependency(T(21)); T(26)->addDependency(T(16)); T(26)->addDependency(T(11)); // skip 2,3
	T(27)->addDependency(T(22)); T(27)->addDependency(T(8));   // skip 4
	T(28)->addDependency(T(23)); T(28)->addDependency(T(17));  // skip 2
	T(29)->addDependency(T(24)); T(29)->addDependency(T(19)); T(29)->addDependency(T(14)); // skip 2,3

	// L6
	T(30)->addDependency(T(26)); T(30)->addDependency(T(21)); T(30)->addDependency(T(6)); // skip 2,5
	T(31)->addDependency(T(27)); T(31)->addDependency(T(22)); T(31)->addDependency(T(12)); // skip 2,4
	T(32)->addDependency(T(28)); T(32)->addDependency(T(18));  // skip 3
	T(33)->addDependency(T(29)); T(33)->addDependency(T(24)); T(33)->addDependency(T(16)); // skip 2,3

	// L7
	T(34)->addDependency(T(30)); T(34)->addDependency(T(26)); T(34)->addDependency(T(21)); // skip 2,3
	T(35)->addDependency(T(31)); T(35)->addDependency(T(27)); T(35)->addDependency(T(13)); // skip 2,5
	T(36)->addDependency(T(32)); T(36)->addDependency(T(33)); T(36)->addDependency(T(28)); T(36)->addDependency(T(22)); // skip 2,3

#ifdef QT_WIDGETS_ENABLED
	TaskGraph::Gui::TaskGraphWidget graphWidget(&scheduler, TaskGraph::Gui::presetEditorFactory());

	QMainWindow win;
	win.setCentralWidget(&graphWidget);
	win.resize(1100, 700);
	win.setWindowTitle("TaskGraph Editor - Large Demo (36 tasks, 8 layers)");

	auto* dock = new QDockWidget("Visuals", &win);
	dock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
	auto* visualsPanel = new VisualConfigPanel(&graphWidget, dock);
	visualsPanel->setMinimumWidth(260);
	dock->setWidget(visualsPanel);
	win.addDockWidget(Qt::LeftDockWidgetArea, dock);

	// Toolbar action toggles the dock (visible by default).
	QToolBar* toolbar = win.addToolBar("Visuals");
	QAction* visualsAction = dock->toggleViewAction();
	visualsAction->setText("Visuals…");
	toolbar->addAction(visualsAction);

	win.show();

	scheduler.runTasksAsync();
#else
	std::this_thread::sleep_for(std::chrono::milliseconds(1000));
	scheduler.runTasks();
#endif


	int ret = 0;
#ifdef QT_ENABLED
	ret = app.exec();
#endif
#ifdef QT_WIDGETS_ENABLED
	// Release the stack-owned central widget so QMainWindow won't delete it.
	win.takeCentralWidget();
#endif
	// Stop any in-progress async run before stack-local widgets and tasks
	// are destroyed — prevents worker threads from emitting into dead objects
	scheduler.cancel();
	while (scheduler.isRunning())
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	TaskGraph::Profiler::stop((std::string(TaskGraph::LibraryInfo::name) + ".prof").c_str());
	return ret;
}

void exiting()
{

}
void exceptionCallback()
{
	//std::cout << "\nCallback reached\n\n";
}
