-- Give Markdown tables real wrapping columns instead of unbounded l columns.
-- Keep the shared WSP builder pinned; this is a project Pandoc filter.
local function escaped(text, code)
  local special = {
    ['\\'] = '\\textbackslash{}', ['{'] = '\\{', ['}'] = '\\}',
    ['$'] = '\\$', ['&'] = '\\&', ['#'] = '\\#', ['%'] = '\\%',
    ['_'] = '\\_', ['~'] = '\\textasciitilde{}',
    ['^'] = '\\textasciicircum{}'
  }
  local parts = {}
  for _, value in utf8.codes(text) do
    local char = utf8.char(value)
    parts[#parts + 1] = special[char] or char
  end
  local result = table.concat(parts, '\\allowbreak{}')
  if code then result = '\\texttt{' .. result .. '}' end
  return pandoc.RawInline('latex', result)
end

function Code(el)
  if FORMAT:match('latex') and (utf8.len(el.text) or 0) > 16 then
    return escaped(el.text, true)
  end
end

function Str(el)
  if FORMAT:match('latex') and (utf8.len(el.text) or 0) > 20 then
    return escaped(el.text, false)
  end
end

function Table(tbl)
  if not FORMAT:match('latex') then return tbl end
  local count = #tbl.colspecs
  if count == 0 then return tbl end
  local weights = {}
  for i = 1, count do weights[i] = 8 end
  local function rows(items)
    for _, row in ipairs(items) do
      local column = 1
      for _, cell in ipairs(row.cells) do
        local span = cell.col_span or 1
        local size = utf8.len(pandoc.utils.stringify(cell.contents)) or 8
        for i = column, math.min(count, column + span - 1) do
          weights[i] = math.max(weights[i], math.min(120, size / span))
        end
        column = column + span
      end
    end
  end
  rows(tbl.head.rows)
  for _, body in ipairs(tbl.bodies) do
    rows(body.head)
    rows(body.body)
  end
  rows(tbl.foot.rows)
  local total = 0
  local explicit = false
  for i, spec in ipairs(tbl.colspecs) do
    total = total + weights[i]
    if (spec[2] or 0) > 0 then explicit = true end
  end
  if not explicit then
    local minimum = math.min(0.14, 0.65 / count)
    local specs = tbl.colspecs
    for i, spec in ipairs(specs) do
      spec[2] = minimum + (1 - minimum * count) * weights[i] / total
      specs[i] = spec
    end
    tbl.colspecs = specs
  end
  -- Paths and identifiers also need break opportunities inside narrow cells.
  return tbl:walk({
    Code = function(el) return escaped(el.text, true) end,
    Str = function(el)
      if (utf8.len(el.text) or 0) > 20 then
        return escaped(el.text, false)
      end
    end
  })
end

-- Compute table widths before converting long inlines to LaTeX.
return {{ traverse = 'topdown', Table = Table, Code = Code, Str = Str }}
