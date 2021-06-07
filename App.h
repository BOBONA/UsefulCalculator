#pragma once

#include "wx/wxprec.h"
#ifndef WX_PRECOMP
#   include "wx/wx.h"
#endif

#include "Calculator.h"

void OnAddInputPressed(wxCommandEvent& event);
void OnRemoveInputPressed(wxCommandEvent& event);
void OnInputEdited(wxCommandEvent& event);

class EvaluatorApp : public wxApp {
public:
    virtual bool OnInit();
    virtual int OnExit();
};

class EvaluatorFrame : public wxFrame {
public:
    static const wxFont inputControlFont;
    Calculator calculator;
    wxScrolledWindow* inputWindow;
    wxBoxSizer* inputSizer;
    wxPanel* panel;
    wxBoxSizer* mainHbox;
    wxTextCtrl* output;

    EvaluatorFrame(const wxString& title);

private:
    void OnOutputClicked(wxMouseEvent& event);
};

class InputEventUserData : public wxObject {
private:
    int id;

public:
    InputEventUserData(int id) : id{ id } {
    }

    int getId() {
        return id;
    }

    virtual ~InputEventUserData() {};
};

DECLARE_APP(EvaluatorApp);