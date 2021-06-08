#include "wx/clipbrd.h"

#include "App.h"

#include <iostream>
#include <string>

IMPLEMENT_APP(EvaluatorApp)

EvaluatorFrame* g_mainFrame;
Calculator* calc;
const wxFont EvaluatorFrame::inputControlFont = wxFontInfo(6).FaceName("Helvetica");

void Reposition(wxSizer* sizer) {
    sizer->RepositionChildren(sizer->CalcMin());
}

void Refresh() {
    Reposition(g_mainFrame->mainHbox);
    g_mainFrame->panel->Refresh();
}

int GetInputIndex(wxCommandEvent& event) {
    wxSizerItemList inputs = g_mainFrame->inputSizer->GetChildren();
    u_int i = 0;
    for (i = 0; i < inputs.size(); ++i) {
        if (inputs[i]->GetId() == ((InputEventUserData*)event.GetEventUserData())->getId()) {
            break;
        }
    }
    return i;
}

void AddInput(bool insert, bool removeOption, int insertIndex = 0) {
    wxPanel* window = g_mainFrame->inputWindow;
    wxBoxSizer* sizer = g_mainFrame->inputSizer;
    static int inputID = -1;
    inputID++;
    wxBoxSizer* container = new wxBoxSizer(wxVERTICAL);
    wxTextCtrl* value = new wxTextCtrl(window, wxID_ANY, "");
    value->Bind(wxEVT_TEXT, &OnInputEdited, wxID_ANY, wxID_ANY, new InputEventUserData(inputID));
    container->Add(value, wxSizerFlags().Expand());
    wxBoxSizer* controls = new wxBoxSizer(wxHORIZONTAL);
    wxButton* add = new wxButton(window, wxID_ANY, "+", wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT);
    add->SetFont(EvaluatorFrame::inputControlFont);
    add->Bind(wxEVT_BUTTON, &OnAddInputPressed, wxID_ANY, wxID_ANY, new InputEventUserData(inputID));
    controls->Add(add);
    wxButton* eval = new wxButton(window, wxID_ANY, L"\U0001F441", wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT);
    eval->SetFont(EvaluatorFrame::inputControlFont);
    controls->Add(eval);
    if (removeOption) {
        wxButton* remove = new wxButton(window, wxID_ANY, "X", wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT);
        remove->SetFont(EvaluatorFrame::inputControlFont);
        remove->Bind(wxEVT_BUTTON, &OnRemoveInputPressed, wxID_ANY, wxID_ANY, new InputEventUserData(inputID));
        controls->Add(remove);
    }
    container->Add(controls, wxSizerFlags().Right());
    container->AddSpacer(10);
    if (insert) {
        sizer->Insert(insertIndex, container, wxSizerFlags().Expand())->SetId(inputID);
    } else {
        sizer->Add(container, wxSizerFlags().Expand())->SetId(inputID);
    }
    wxLogDebug("%d", insertIndex);
    calc->AddLine(insertIndex);
}

void OnAddInputPressed(wxCommandEvent& event) {
    int index = GetInputIndex(event);
    AddInput(true, true, index + 1);
    Refresh();
}

void OnRemoveInputPressed(wxCommandEvent& event) {
    int index = GetInputIndex(event);
    g_mainFrame->inputSizer->Hide(index);
    bool remove = g_mainFrame->inputSizer->Remove(index);
    calc->RemoveLine(index);
    Reposition(g_mainFrame->inputSizer);
    Refresh();
}

void OnInputEdited(wxCommandEvent& event) {
    int index = GetInputIndex(event);
    wxSizer* sizer = g_mainFrame->inputSizer->GetChildren()[index]->GetSizer();
    wxTextCtrl* input = wxStaticCast(sizer->GetChildren()[0]->GetWindow(), wxTextCtrl);
    try {
        calc->ParseLine(input->GetValue(), index);
        g_mainFrame->output->SetValue(calc->GetFormattedLine(index));
    } catch (std::runtime_error e) {
        g_mainFrame->output->SetValue(e.what());
    }
}

bool EvaluatorApp::OnInit() {
    EvaluatorFrame* frame = new EvaluatorFrame("Evaluator");
    frame->Show(true);
    return true;
}

int EvaluatorApp::OnExit() {
    return wxApp::OnExit();
}

EvaluatorFrame::EvaluatorFrame(const wxString& title)
    : wxFrame(NULL, wxID_ANY, title, wxPoint(-1, -1), wxSize(500, 500)), inputWindow{ NULL }, inputSizer{ NULL } {
    g_mainFrame = this;
    calc = &g_mainFrame->calculator;
    panel = new wxPanel(this);
    mainHbox = new wxBoxSizer(wxHORIZONTAL);
    wxBoxSizer* left = new wxBoxSizer(wxVERTICAL);
    wxStaticText* labelI = new wxStaticText(panel, wxID_ANY, "Input", wxDefaultPosition, wxDefaultSize, wxALIGN_CENTRE_HORIZONTAL);
    left->Add(labelI, wxSizerFlags().Expand());
    inputWindow = new wxScrolledWindow(panel);
    inputSizer = new wxBoxSizer(wxVERTICAL);
    inputWindow->SetSizer(inputSizer);
    inputWindow->SetScrollRate(0, 10);
    AddInput(false, false);
    left->Add(inputWindow, wxSizerFlags().Proportion(1).Expand());
    mainHbox->Add(left, wxSizerFlags().Proportion(1).Expand());
        
    wxBoxSizer* right = new wxBoxSizer(wxVERTICAL);
    wxStaticText* labelO = new wxStaticText(panel, wxID_ANY, "Output", wxDefaultPosition, wxDefaultSize, wxALIGN_CENTRE_HORIZONTAL);
    right->Add(labelO, wxSizerFlags().Expand());
    output = new wxTextCtrl(panel, wxID_ANY, "123982", wxDefaultPosition, wxDefaultSize, wxTE_READONLY);
    output->Bind(wxEVT_LEFT_DOWN, &EvaluatorFrame::OnOutputClicked, this);
    right->Add(output, wxSizerFlags().Expand());
    mainHbox->Add(right, wxSizerFlags().Proportion(1).Expand());

    panel->SetSizer(mainHbox);

    CreateStatusBar();
    SetStatusText("Evaluator");
}

void EvaluatorFrame::OnOutputClicked(wxMouseEvent& event) {
    wxTextCtrl* text = wxStaticCast(event.GetEventObject(), wxTextCtrl);
    if (wxTheClipboard->Open()) {
        wxTheClipboard->SetData(new wxTextDataObject(text->GetValue()));
        wxTheClipboard->Close();
    }
}