/*
 * @Author: DI JUNKUN
 * @Date: 2026-10-15
 * Copyright (c) 2026 by DI JUNKUN, All Rights Reserved.
 */

#ifndef _MODEL_UPDATE_H_
#define _MODEL_UPDATE_H_

#include <slint.h>

#include <utility>
#include <vector>

namespace crossdesk::ui {

// A reset invalidates row-count bindings even if the count is unchanged.
// Repeating it from a timer can continually restart dependent size animations.
template <typename Row>
void UpdateModelRows(slint::VectorModel<Row>& model, std::vector<Row> rows) {
  if (model.row_count() != rows.size()) {
    model.set_vector(std::move(rows));
    return;
  }
  for (std::size_t i = 0; i < rows.size(); ++i) {
    if (model.row_data(i) != rows[i]) {
      model.set_row_data(i, rows[i]);
    }
  }
}

}  // namespace crossdesk::ui

#endif
