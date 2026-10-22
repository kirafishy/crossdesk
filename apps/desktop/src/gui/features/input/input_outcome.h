/*
 * @Author: DI JUNKUN
 * @Date: 2026-10-22
 * Copyright (c) 2026 by DI JUNKUN, All Rights Reserved.
 */

#ifndef _INPUT_OUTCOME_H_
#define _INPUT_OUTCOME_H_

namespace crossdesk {

// Applied means the native/service API accepted the operation. It does not
// assert that the foreground application has consumed the injected event.
enum class InputOutcome { applied, ignored, failed };

inline const char* InputOutcomeName(InputOutcome outcome) {
  switch (outcome) {
    case InputOutcome::applied:
      return "applied";
    case InputOutcome::ignored:
      return "ignored";
    case InputOutcome::failed:
      return "failed";
  }
  return "failed";
}

}  // namespace crossdesk
#endif