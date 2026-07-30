#include "gui/GuiPromptService.h"

#if defined(QT_WIDGETS_ENABLED)
#include "TaskScheduler.h"
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>

namespace TaskGraph
{
namespace Gui
{
    GuiPromptService::GuiPromptService(TaskScheduler* scheduler, QWidget* parentWidget, QObject* parent)
        : QObject(parent)
        , m_scheduler(scheduler)
        , m_parentWidget(parentWidget)
    {
        connect(m_scheduler, &TaskScheduler::guiEventRequested,
                this, &GuiPromptService::onGuiEventRequested);
        connect(m_scheduler, &TaskScheduler::cancelled,
                this, &GuiPromptService::onCancelled);
    }

    void GuiPromptService::onGuiEventRequested(int requestId, QString taskName, QVariant payload)
    {
        // dismiss any existing prompt
        if (m_activeDialog)
        {
            m_activeDialog->reject();
            m_activeDialog->deleteLater();
            m_activeDialog = nullptr;
        }

        auto* dlg = new QDialog(m_parentWidget);
        dlg->setWindowTitle("Task Prompt: " + taskName);
        dlg->setModal(false);
        dlg->setAttribute(Qt::WA_DeleteOnClose, false);

        auto* layout = new QVBoxLayout(dlg);

        auto* label = new QLabel(
            "Task <b>" + taskName + "</b> requests input:", dlg);
        layout->addWidget(label);

        auto* payloadLabel = new QLabel(payload.toString(), dlg);
        payloadLabel->setWordWrap(true);
        layout->addWidget(payloadLabel);

        auto* input = new QLineEdit(dlg);
        input->setPlaceholderText("Enter response...");
        layout->addWidget(input);

        auto* btnRow = new QHBoxLayout();
        auto* okBtn = new QPushButton("OK", dlg);
        auto* cancelBtn = new QPushButton("Cancel", dlg);
        btnRow->addStretch(1);
        btnRow->addWidget(okBtn);
        btnRow->addWidget(cancelBtn);
        layout->addLayout(btnRow);

        int capturedId = requestId;

        connect(okBtn, &QPushButton::clicked, dlg, [this, dlg, input, capturedId]() {
            QString text = input->text();
            dlg->close();
            m_activeDialog = nullptr;
            m_scheduler->respondToGuiEvent(capturedId, QVariant(text));
        });

        connect(cancelBtn, &QPushButton::clicked, dlg, [this, dlg, capturedId]() {
            dlg->close();
            m_activeDialog = nullptr;
            m_scheduler->respondToGuiEvent(capturedId, QVariant());
        });

        m_activeDialog = dlg;
        dlg->resize(350, 180);
        dlg->show();
    }

    void GuiPromptService::onCancelled()
    {
        if (m_activeDialog)
        {
            m_activeDialog->close();
            m_activeDialog->deleteLater();
            m_activeDialog = nullptr;
        }
    }
}
}

#endif
